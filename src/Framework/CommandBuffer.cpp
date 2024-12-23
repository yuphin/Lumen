#include "../LumenPCH.h"
#include "CommandBuffer.h"

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

namespace sync {
std::mutex queue_mutex;
std::mutex command_pool_mutex;
uint64_t available_command_pools = UINT64_MAX;
std::counting_semaphore<64> command_pool_semaphore{64};
}  // namespace sync

CommandBuffer::CommandBuffer(bool begin, VkCommandBufferUsageFlags begin_flags, vk::QueueType type,
							 VkCommandBufferLevel level) {
	this->type = type;
	std::unique_lock<std::mutex> cv_lock;

	sync::command_pool_semaphore.acquire();
	{
		std::scoped_lock lock(sync::command_pool_mutex);
		curr_tid = get_first_available_tid(sync::available_command_pools);
		sync::available_command_pools &= ~(uint64_t(1) << curr_tid);
	}
	auto cmd_buf_allocate_info = vk::command_buffer_allocate_info(vk::context().cmd_pools[curr_tid], level, 1);
	vk::check(vkAllocateCommandBuffers(vk::context().device, &cmd_buf_allocate_info, &handle));
	if (begin) {
		auto begin_info = vk::command_buffer_begin_info(begin_flags);
		vk::check(vkBeginCommandBuffer(handle, &begin_info));
		state = CommandBufferState::RECORDING;
	}
}

void CommandBuffer::begin(VkCommandBufferUsageFlags begin_flags) {
	LUMEN_ASSERT(state != CommandBufferState::RECORDING, "Command buffer is already recording");
	std::unique_lock<std::mutex> cv_lock;
	sync::command_pool_semaphore.acquire();
	if (curr_tid == -1) {
		std::scoped_lock lock(sync::command_pool_mutex);
		curr_tid = get_first_available_tid(sync::available_command_pools);
		sync::available_command_pools &= ~(uint64_t(1) << curr_tid);
	}
	auto begin_info = vk::command_buffer_begin_info(begin_flags);
	vk::check(vkBeginCommandBuffer(handle, &begin_info));
	state = CommandBufferState::RECORDING;
}

void CommandBuffer::submit(bool wait_fences, bool queue_wait_idle) {
	vk::check(vkEndCommandBuffer(handle));
	state = CommandBufferState::STOPPED;
	VkSubmitInfo submit_info = vk::submit_info();
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &handle;
	sync::queue_mutex.lock();
	if (wait_fences) {
		VkFenceCreateInfo fence_info = vk::fence();
		VkFence fence;
		vkCreateFence(vk::context().device, &fence_info, nullptr, &fence);
		vk::check(vkQueueSubmit(vk::context().queues[(int)type], 1, &submit_info, fence));
		vk::check(vkWaitForFences(vk::context().device, 1, &fence, VK_TRUE, ~0ull));
		vkDestroyFence(vk::context().device, fence, nullptr);
	} else {
		vk::check(vkQueueSubmit(vk::context().queues[(int)type], 1, &submit_info, VK_NULL_HANDLE));
	}
	if (queue_wait_idle) {
		vk::check(vkQueueWaitIdle(vk::context().queues[(int)type]));
	}
	sync::queue_mutex.unlock();
}

CommandBuffer::~CommandBuffer() {
	if (handle == VK_NULL_HANDLE) {
		return;
	}
	if (state == CommandBufferState::RECORDING) {
		LUMEN_WARN("Destroying command buffer in recording state.");
		vk::check(vkEndCommandBuffer(handle));
	}
	vkFreeCommandBuffers(vk::context().device, vk::context().cmd_pools[curr_tid], 1, &handle);
	{
		std::scoped_lock lock(sync::command_pool_mutex);
		sync::available_command_pools |= uint64_t(1) << curr_tid;
	}
	sync::command_pool_semaphore.release();
}

}  // namespace vk
