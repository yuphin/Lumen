#pragma once
#include "LumenPCH.h"
#include "CommonTypes.h"
#include "Buffer.h"

class RenderPass;


struct Shader {
	std::vector<uint32_t> binary;
	std::string filename;

	VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	VkDescriptorType descriptor_types[32] = {};
	uint32_t binding_mask = 0;
	
	int local_size_x = 1;
	int local_size_y = 1;
	int local_size_z = 1;
	bool uses_push_constants = false;
	uint32_t push_constant_size = 0;
	Shader();
	Shader(const std::string& filename);
	int compile(RenderPass* pass);
	VkShaderModule create_vk_shader_module(const VkDevice& device) const;
	std::vector<std::pair<VkFormat, uint32_t>> vertex_inputs;
	robin_hood::unordered_map<Buffer*, BufferStatus> buffer_status_map;
};
