#include "lmhpch.h"
#include "CommandBuffer.h"

CommandBuffer::CommandBuffer(VkCommandBufferLevel level, bool begin) {
	device = &Lumen::get()->get_device();
	pool = &Lumen::get()->get_command_pool();
	auto cmd_buf_allocate_info = vks::command_buffer_allocate_info(
		Lumen::get()->get_command_pool(),
		level, 1
	);

	vks::check(
		vkAllocateCommandBuffers(*device,
			&cmd_buf_allocate_info,
			&handle),
		"Could not allocate command buffer"
	);
	if (begin) {
		auto begin_info = vks::command_buffer_begin_info();
		vks::check(vkBeginCommandBuffer(handle, &begin_info),
			"Could not begin the command buffer"
		);
		state = CommandBufferState::RECORDING;
	}


}

CommandBuffer::~CommandBuffer() {
	if (handle == VK_NULL_HANDLE) {
		return;
	}
	if (state == CommandBufferState::RECORDING) {
		vks::check(vkEndCommandBuffer(handle), "Failed to end command buffer");
	}
	vkFreeCommandBuffers(*device, *pool, 1, &handle);
}

void CommandBuffer::submit(VkQueue queue, bool wait_fences) {
	vks::check(vkEndCommandBuffer(handle), "Failed to end command buffer");
	state = CommandBufferState::STOPPED;
	VkSubmitInfo submit_info = vks::submit_info();
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &handle;
	if (wait_fences) {
		VkFenceCreateInfo fence_info = vks::fence_create_info(0);
		VkFence fence;
		vks::check(vkCreateFence(*device, &fence_info, nullptr, &fence),
			"Fence creation error"
		);
		vks::check(vkQueueSubmit(queue, 1, &submit_info, fence), "Queue submission error");
		vks::check(vkWaitForFences(*device, 1, &fence, VK_TRUE, 100000000000), "Fence wait error");
		vkDestroyFence(*device, fence, nullptr);
	}
	else {
		vks::check(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE), "Queue submission error");
	}

}
