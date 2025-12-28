#pragma once
#include "Buffer.h"
#include "Framework/Base/String.h"
#include "Framework/Base/OS.h"
#include "Framework/Base/Memory.h"
#include "Framework/Base/HashMap.h"
#include <Framework/Base/SmallArray.h>

namespace lm {
class RenderPass;
}
namespace vk {
////////////////////////////
// --- Limits ---
static constexpr u64 MAX_VERTEX_INPUTS = 8;
struct BindingStatus {
	bool read = false;
	bool write = false;
	bool active = false;
};

struct Shader {
	Shader() = default;
	Shader(const lm::String& filename);
	std::vector<u32> binary;
	lm::String filename;
	lm::String name_with_macros;

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
	u32 num_as_bindings = 0;

	lm::SmallArray<std::pair<VkFormat, u32>, MAX_VERTEX_INPUTS> vertex_inputs;
	lm::HashMap<lm::String, BufferStatus> buffer_status_map;
	lm::HashMap<u32, BindingStatus> resource_binding_map;

};

}  // namespace vk