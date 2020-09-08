#pragma once
#include "lmhpch.h"
#include "gfx/vulkan/Base.h"
#include "Lumen.h"



class CommandBuffer {
public:
	CommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				  bool begin = false);
	~CommandBuffer();
	void submit(VkQueue queue, bool wait_fences = true );

	VkCommandBuffer handle = VK_NULL_HANDLE;
private:
	enum class CommandBufferState {
		RECORDING,
		STOPPED
	};
	VkDevice* device = nullptr;
	VkCommandPool* pool = nullptr;

	CommandBufferState state = CommandBufferState::STOPPED;
	
};

