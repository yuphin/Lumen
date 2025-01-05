#pragma once
#include "../LumenPCH.h"
#include "CommonTypes.h"
#include "Buffer.h"

namespace lumen {
class RenderPass;
}
namespace vk {

struct Shader {
	Shader() = default;
	Shader(const std::string& filename);
	std::vector<uint32_t> binary;
	std::string filename;
	std::string name_with_macros;

	VkShaderStageFlagBits stage = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
	VkDescriptorType descriptor_types[32] = {};
	uint32_t binding_mask = 0;

	int local_size_x = 1;
	int local_size_y = 1;
	int local_size_z = 1;
	bool uses_push_constants = false;
	uint32_t push_constant_size = 0;
	int compile(lumen::RenderPass* pass);
	VkShaderModule create_vk_shader_module(const VkDevice& device) const;
	struct BindingStatus {
		bool read = false;
		bool write = false;
		bool active = false;
	};
	std::vector<std::pair<VkFormat, uint32_t>> vertex_inputs;
	std::unordered_map<std::string, BufferStatus> buffer_status_map;
	std::unordered_map<uint32_t, BindingStatus> resource_binding_map;
	uint32_t num_as_bindings = 0;
};

}  // namespace vk