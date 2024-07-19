#include "../LumenPCH.h"
#include "CommandBuffer.h"

namespace lumen {
CommandBuffer::CommandBuffer(VulkanContext* ctx, bool begin, VkCommandBufferUsageFlags begin_flags, QueueType type,
							 VkCommandBufferLevel level) {
	this->ctx = ctx;
	this->type = type;
	std::unique_lock<std::mutex> cv_lock;
	VulkanSyncronization::cv.wait(cv_lock, [&] { return VulkanSyncronization::available_command_pools > 0; });
	curr_tid = --VulkanSyncronization::available_command_pools;
	auto cmd_buf_allocate_info = vk::command_buffer_allocate_info(ctx->cmd_pools[curr_tid], level, 1);
	vk::check(vkAllocateCommandBuffers(ctx->device, &cmd_buf_allocate_info, &handle),
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
		vk::check(vkCreateFence(ctx->device, &fence_info, nullptr, &fence), "Fence creation error");
		vk::check(vkQueueSubmit(ctx->queues[(int)type], 1, &submit_info, fence), "Queue submission error");
		vk::check(vkWaitForFences(ctx->device, 1, &fence, VK_TRUE, 100000000000), "Fence wait error");
		vkDestroyFence(ctx->device, fence, nullptr);
	} else {
		vk::check(vkQueueSubmit(ctx->queues[(int)type], 1, &submit_info, VK_NULL_HANDLE), "Queue submission error");
	}
	if (queue_wait_idle) {
		vk::check(vkQueueWaitIdle(ctx->queues[(int)type]), "Queue wait error! Check previous submissions");
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
	vkFreeCommandBuffers(ctx->device, ctx->cmd_pools[curr_tid], 1, &handle);
}

}  // namespace lumen
