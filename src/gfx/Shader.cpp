#include "lmhpch.h"
#include "Shader.h"
Shader::Shader() {}
Shader::Shader(const std::string& filename) : filename(filename) {
	compile();
}
void Shader::compile() {
	binary.clear();
	LUMEN_TRACE("Compiling shader: {0}", filename);
	auto str = std::string("glslc.exe " + filename + " -o " + filename + ".spv");
	std::system(str.data());
	std::ifstream bin(filename + ".spv", std::ios::ate | std::ios::binary);
	if (!bin.good()) {
		LUMEN_ERROR(
			std::string("Failed to compile shader " + filename).data());
	}
	size_t file_size = (size_t)bin.tellg();
	bin.seekg(0);
	binary.resize(file_size);
	bin.read(binary.data(), file_size);
	bin.close();

}

VkShaderModule Shader::create_vk_shader_module(const VkDevice& device) {
	VkShaderModuleCreateInfo shader_module_CI{};
	shader_module_CI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_module_CI.codeSize = binary.size();
	shader_module_CI.pCode = reinterpret_cast<const uint32_t*>(binary.data());

	VkShaderModule shader_module;
	if (vkCreateShaderModule(device, &shader_module_CI, nullptr, &shader_module) != VK_SUCCESS) {
		LUMEN_ERROR("Failed to create shader module!");
	}

	return shader_module;
}