#pragma once
#include "Buffer.h"

namespace lm {
class RenderPass;
}
namespace vk {

struct Shader {
	Shader() = default;
	Shader(const std::string& filename);
	std::vector<u32> binary;
	std::string filename;
	std::string name_with_macros;

	VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	VkDescriptorType descriptor_types[32] = {};
	u32 binding_mask = 0;

	i32 local_size_x = 1;
	i32 local_size_y = 1;
	i32 local_size_z = 1;
	bool uses_push_constants = false;
	u32 push_constant_size = 0;
	i32 compile(lm::RenderPass* pass);
	VkShaderModule create_vk_shader_module(const VkDevice& device) const;
	struct BindingStatus {
		bool read = false;
		bool write = false;
		bool active = false;
	};
	std::vector<std::pair<VkFormat, u32>> vertex_inputs;
	std::unordered_map<std::string, BufferStatus> buffer_status_map;
	std::unordered_map<u32, BindingStatus> resource_binding_map;
	u32 num_as_bindings = 0;
};

}  // namespace vk