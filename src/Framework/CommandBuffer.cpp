#include "../LumenPCH.h"
#include "CommandBuffer.h"
#include "VulkanSyncronization.h"

namespace vk {
CommandBuffer::CommandBuffer(bool begin, VkCommandBufferUsageFlags begin_flags, vk::QueueType type,
							 VkCommandBufferLevel level) {
	this->type = type;
	std::unique_lock<std::mutex> cv_lock;
	VulkanSyncronization::cv.wait(cv_lock, [&] { return VulkanSyncronization::available_command_pools > 0; });
	curr_tid = --VulkanSyncronization::available_command_pools;
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
	VulkanSyncronization::cv.wait(cv_lock, [&] { return VulkanSyncronization::available_command_pools > 0; });
	--VulkanSyncronization::available_command_pools;
	auto begin_info = vk::command_buffer_begin_info(begin_flags);
	vk::check(vkBeginCommandBuffer(handle, &begin_info), "Could not begin the command buffer");
	state = CommandBufferState::RECORDING;
}

void CommandBuffer::submit(bool wait_fences, bool queue_wait_idle) {
	vk::check(vkEndCommandBuffer(handle), "Failed to end command buffer");
	++VulkanSyncronization::available_command_pools;
	VulkanSyncronization::cv.notify_one();
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
		vk::check(vkEndCommandBuffer(handle), "Failed to end command buffer");
		++VulkanSyncronization::available_command_pools;
		VulkanSyncronization::cv.notify_one();
	}
	vkFreeCommandBuffers(vk::context().device, vk::context().cmd_pools[curr_tid], 1, &handle);
}

}  // namespace vk
