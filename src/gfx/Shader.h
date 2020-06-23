#pragma once
#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include <cstdlib>
#include <cstdio>
struct Shader {
	std::vector<char> binary;
	std::string filename;

	Shader(){}
	Shader(const std::string& filename) : filename(filename) {
		compile();
	}
	void compile() {
		binary.clear();
		std::cout << "Compiling shader: " << filename << std::endl;
		auto str = std::string("glslc.exe " + filename + " -o " + filename + ".spv");
		std::system(str.data());
		std::ifstream bin(filename + ".spv", std::ios::ate | std::ios::binary);
		if (!bin.good()) {
			throw std::runtime_error(
				std::string("Failed to compile shader " + filename).data());
		}
		size_t file_size = (size_t)bin.tellg();
		bin.seekg(0);
		binary.resize(file_size);
		bin.read(binary.data(), file_size);
		bin.close();

	}

	VkShaderModule create_vk_shader_module(const VkDevice& device) {
		VkShaderModuleCreateInfo shader_module_CI{};
		shader_module_CI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shader_module_CI.codeSize = binary.size();
		shader_module_CI.pCode = reinterpret_cast<const uint32_t*>(binary.data());

		VkShaderModule shader_module;
		if (vkCreateShaderModule(device, &shader_module_CI, nullptr, &shader_module) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module!");
		}

		return shader_module;
	}



};
