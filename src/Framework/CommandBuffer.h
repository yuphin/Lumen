#pragma once
#include "LumenPCH.h"
#include "Framework/VulkanBase.h"



class CommandBuffer {
public:
	CommandBuffer(VulkanContext* ctx, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				  bool begin = false);
	~CommandBuffer();
	void submit(VkQueue queue, bool wait_fences = true);

	VkCommandBuffer handle = VK_NULL_HANDLE;
private:
	enum class CommandBufferState {
		RECORDING,
		STOPPED
	};
	VulkanContext* ctx;
	CommandBufferState state = CommandBufferState::STOPPED;
};
