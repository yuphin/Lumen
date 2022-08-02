#pragma once
#include "LumenPCH.h"

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
	Shader();
	Shader(const std::string& filename);
	int compile();
	VkShaderModule create_vk_shader_module(const VkDevice& device) const;
	std::vector<std::pair<VkFormat, size_t>> vertex_inputs;
};
