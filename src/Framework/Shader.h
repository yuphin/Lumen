#pragma once
#include "LumenPCH.h"
struct Shader {
	std::vector<char> binary;
	std::string filename;

	Shader();
	Shader(const std::string& filename);
	int compile();
	VkShaderModule create_vk_shader_module(const VkDevice& device) const;
};
