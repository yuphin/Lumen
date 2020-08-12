#pragma once
#include "lmhpch.h"
struct Shader {
	std::vector<char> binary;
	std::string filename;

	Shader();
	Shader(const std::string& filename);
	void compile();
	VkShaderModule create_vk_shader_module(const VkDevice& device);


};
