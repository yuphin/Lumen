#pragma once
#include "LumenPCH.h"

class CommandBuffer {
   public:
	CommandBuffer(VulkanContext* ctx, bool begin = false,
				  VkCommandBufferUsageFlags begin_flags = 0,
				  QueueType type = QueueType::GFX,
				  VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	~CommandBuffer();
	void begin(VkCommandBufferUsageFlags begin_flags = 0);
	void submit(bool wait_fences = true, bool queue_wait_idle = true);

	VkCommandBuffer handle = VK_NULL_HANDLE;

   private:
	enum class CommandBufferState { RECORDING, STOPPED };
	VulkanContext* ctx;
	CommandBufferState state = CommandBufferState::STOPPED;
	QueueType type;
};
