#include "LumenPCH.h"
#include "Shader.h"
Shader::Shader() {}
Shader::Shader(const std::string& filename) : filename(filename) {}
int Shader::compile() {
	std::string file_path = filename + ".spv";
#ifdef NDEBUG
	auto str = std::string("glslangValidator.exe --target-env vulkan1.2 " + filename + "-V " + " -o " + filename + ".spv");
#else 
	auto str = std::string("glslangValidator.exe --target-env vulkan1.2 " + filename + " -V " +  " -g " + " -o " + filename + ".spv");
#endif //  NDEBUG

	binary.clear();
	LUMEN_TRACE("Compiling shader: {0}", filename);
	int ret_val = std::system(str.data());
	std::ifstream bin(file_path, std::ios::ate | std::ios::binary);
	if(!bin.good() && ret_val) {
		LUMEN_CRITICAL(
			std::string("Shader compilation failed: " + filename).data());
		bin.close();
		return ret_val;
	} else if(ret_val) {
		LUMEN_WARN(
			std::string("Shader compilation failed, resuming from old shader: " + filename).data());
	}
	size_t file_size = (size_t) bin.tellg();
	bin.seekg(0);
	binary.resize(file_size);
	bin.read(binary.data(), file_size);
	bin.close();
	return ret_val;
}

VkShaderModule Shader::create_vk_shader_module(const VkDevice& device) const {
	VkShaderModuleCreateInfo shader_module_CI{};
	shader_module_CI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_module_CI.codeSize = binary.size();
	shader_module_CI.pCode = reinterpret_cast<const uint32_t*>(binary.data());
	shader_module_CI.pNext = nullptr;

	VkShaderModule shader_module;
	if(vkCreateShaderModule(device, &shader_module_CI, nullptr, &shader_module) != VK_SUCCESS) {
		LUMEN_ERROR("Failed to create shader module!");
	}
	return shader_module;
}