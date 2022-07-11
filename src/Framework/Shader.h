#pragma once
#include "LumenPCH.h"
#if VK_HEADER_VERSION >= 135
#include <spirv-headers/spirv.h>
#else
#include <vulkan/spirv.h>
#endif
struct Shader {
	std::vector<uint32_t> binary;
	std::string filename;


	VkShaderStageFlagBits stage;
	VkDescriptorType descriptor_types[32];
	uint32_t binding_mask;
	
	int local_size_x;
	int local_size_y;
	int local_size_z;
	bool uses_push_constants = false;

	VkDescriptorUpdateTemplate update_template;

	Shader();
	Shader(const std::string& filename);
	int compile();
	VkShaderModule create_vk_shader_module(const VkDevice& device) const;
	VkDescriptorUpdateTemplate create_update_template()
};
