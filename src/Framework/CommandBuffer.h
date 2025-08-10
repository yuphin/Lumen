#pragma once

#include "Framework/VulkanContext.h"

namespace vk {
class CommandBuffer {
   public:
	CommandBuffer(bool begin = false, VkCommandBufferUsageFlags begin_flags = 0,
				  vk::QueueType type = vk::QueueType::GFX,
				  VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	~CommandBuffer();
	void begin(VkCommandBufferUsageFlags begin_flags = 0);
	void submit(bool wait_fences = true, bool queue_wait_idle = true);

	VkCommandBuffer handle = VK_NULL_HANDLE;

   private:
	enum class CommandBufferState { RECORDING, STOPPED };
	CommandBufferState state = CommandBufferState::STOPPED;
	vk::QueueType type;
	u32 curr_tid = -1;
};

}  // namespace vk
