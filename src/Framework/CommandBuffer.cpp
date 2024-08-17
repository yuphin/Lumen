#include "../LumenPCH.h"
#include "CommandBuffer.h"
#include "VulkanSyncronization.h"

static uint32_t get_first_available_tid(uint64_t val) {
#ifdef _MSC_VER
	unsigned long index;
	_BitScanForward64(&index, val);
	return uint32_t(index);
#else
	return uint32_t(__builtin_ctzll(val));
#endif
}
namespace vk {
CommandBuffer::CommandBuffer(bool begin, VkCommandBufferUsageFlags begin_flags, vk::QueueType type,
							 VkCommandBufferLevel level) {
	this->type = type;
	std::unique_lock<std::mutex> cv_lock;

	VulkanSyncronization::command_pool_semaphore.acquire();
	{
		std::scoped_lock lock(VulkanSyncronization::command_pool_mutex);
		curr_tid = get_first_available_tid(VulkanSyncronization::available_command_pools);
		VulkanSyncronization::available_command_pools &= ~(uint64_t(1) << curr_tid);
	}
	auto cmd_buf_allocate_info = vk::command_buffer_allocate_info(vk::context().cmd_pools[curr_tid], level, 1);
	vk::check(vkAllocateCommandBuffers(vk::context().device, &cmd_buf_allocate_info, &handle),
			  "Could not allocate command buffer");
	if (begin) {
		auto begin_info = vk::command_buffer_begin_info(begin_flags);
		vk::check(vkBeginCommandBuffer(handle, &begin_info), "Could not begin the command buffer");
		state = CommandBufferState::RECORDING;
	}
}

void CommandBuffer::begin(VkCommandBufferUsageFlags begin_flags) {
	LUMEN_ASSERT(state != CommandBufferState::RECORDING, "Command buffer is already recording");
	std::unique_lock<std::mutex> cv_lock;
	VulkanSyncronization::command_pool_semaphore.acquire();
	{
		std::scoped_lock lock(VulkanSyncronization::command_pool_mutex);
		curr_tid = get_first_available_tid(VulkanSyncronization::available_command_pools);
		VulkanSyncronization::available_command_pools &= ~(uint64_t(1) << curr_tid);
	}
	auto begin_info = vk::command_buffer_begin_info(begin_flags);
	vk::check(vkBeginCommandBuffer(handle, &begin_info), "Could not begin the command buffer");
	state = CommandBufferState::RECORDING;
}

void CommandBuffer::submit(bool wait_fences, bool queue_wait_idle) {
	vk::check(vkEndCommandBuffer(handle), "Failed to end command buffer");
	state = CommandBufferState::STOPPED;
	VkSubmitInfo submit_info = vk::submit_info();
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &handle;
	VulkanSyncronization::queue_mutex.lock();
	if (wait_fences) {
		VkFenceCreateInfo fence_info = vk::fence();
		VkFence fence;
		vk::check(vkCreateFence(vk::context().device, &fence_info, nullptr, &fence), "Fence creation error");
		vk::check(vkQueueSubmit(vk::context().queues[(int)type], 1, &submit_info, fence), "Queue submission error");
		vk::check(vkWaitForFences(vk::context().device, 1, &fence, VK_TRUE, 100000000000), "Fence wait error");
		vkDestroyFence(vk::context().device, fence, nullptr);
	} else {
		vk::check(vkQueueSubmit(vk::context().queues[(int)type], 1, &submit_info, VK_NULL_HANDLE),
				  "Queue submission error");
	}
	if (queue_wait_idle) {
		vk::check(vkQueueWaitIdle(vk::context().queues[(int)type]), "Queue wait error! Check previous submissions");
	}
	VulkanSyncronization::queue_mutex.unlock();
}

CommandBuffer::~CommandBuffer() {
	if (handle == VK_NULL_HANDLE) {
		return;
	}
	if (state == CommandBufferState::RECORDING) {
		LUMEN_WARN("Destroying command buffer in recording state.");
		vk::check(vkEndCommandBuffer(handle), "Failed to end command buffer");
	}
	vkFreeCommandBuffers(vk::context().device, vk::context().cmd_pools[curr_tid], 1, &handle);
	{
		std::scoped_lock lock(VulkanSyncronization::command_pool_mutex);
		VulkanSyncronization::available_command_pools |= uint64_t(1) << curr_tid;
	}
	VulkanSyncronization::command_pool_semaphore.release();
}

}  // namespace vk
