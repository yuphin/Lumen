#pragma once
#include "LumenPCH.h"
#include "Framework/VulkanBase.h"

class CommandBuffer {
public:
	CommandBuffer(VulkanContext* ctx, bool begin = false,
				  QueueType type = QueueType::GFX,
				  VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	~CommandBuffer();
	void submit(bool wait_fences = true);

	VkCommandBuffer handle = VK_NULL_HANDLE;

private:
	enum class CommandBufferState { RECORDING, STOPPED };
	VulkanContext* ctx;
	CommandBufferState state = CommandBufferState::STOPPED;
	QueueType type;
};
