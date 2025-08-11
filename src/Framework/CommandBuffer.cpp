#include "CommandBuffer.h"

static u32 get_first_available_tid(u64 val) {
#ifdef _MSC_VER
	unsigned long index;
	_BitScanForward64(&index, val);
	return u32(index);
#else
	return u32(__builtin_ctzll(val));
#endif
}
namespace vk {

std::mutex queue_mutex;
std::mutex command_pool_mutex;
u64 available_command_pools = UINT64_MAX;
std::counting_semaphore<64> command_pool_semaphore{64};

CommandBuffer::CommandBuffer(bool begin, VkCommandBufferUsageFlags begin_flags, vk::QueueType type,
							 VkCommandBufferLevel level) {
	this->type = type;
	std::unique_lock<std::mutex> cv_lock;

	command_pool_semaphore.acquire();
	{
		std::scoped_lock lock(command_pool_mutex);
		curr_tid = get_first_available_tid(available_command_pools);
		available_command_pools &= ~(u64(1) << curr_tid);
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
	if (curr_tid == -1) {
		std::unique_lock<std::mutex> cv_lock;
		command_pool_semaphore.acquire();
		{
			std::scoped_lock lock(command_pool_mutex);
			curr_tid = get_first_available_tid(available_command_pools);
			available_command_pools &= ~(u64(1) << curr_tid);
		}
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
	queue_mutex.lock();
	if (wait_fences) {
		VkFenceCreateInfo fence_info = vk::fence();
		VkFence fence;
		vkCreateFence(vk::context().device, &fence_info, nullptr, &fence);
		vk::check(vkQueueSubmit(vk::context().queues[(i32)type], 1, &submit_info, fence));
		vk::check(vkWaitForFences(vk::context().device, 1, &fence, VK_TRUE, ~0ull));
		vkDestroyFence(vk::context().device, fence, nullptr);
	} else {
		vk::check(vkQueueSubmit(vk::context().queues[(i32)type], 1, &submit_info, VK_NULL_HANDLE));
	}
	if (queue_wait_idle) {
		vk::check(vkQueueWaitIdle(vk::context().queues[(i32)type]));
	}
	queue_mutex.unlock();
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
		std::scoped_lock lock(command_pool_mutex);
		available_command_pools |= u64(1) << curr_tid;
	}
	command_pool_semaphore.release();
}

}  // namespace vk
