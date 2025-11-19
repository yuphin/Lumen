#pragma once
#include "Shader.h"
#include "Buffer.h"
#include "Texture.h"
#include "Framework/Base/String.h"
#include "Framework/Base/OS.h"
#include "Framework/Base/Memory.h"
#include "Framework/Base/HashMap.h"
#include "Framework/Base/SmallArray.h"

namespace lm {
class RenderPass;


static constexpr u64 MAX_SPEC_CONSTANTS = 8;
static constexpr u64 MAX_SHADERS_PER_PASS = 8;
using SpecializationConstantArray = SmallArray<u32, MAX_SPEC_CONSTANTS>;

struct dim3 {
	u32 x = 1;
	u32 y = 1;
	u32 z = 1;
};

struct RenderGraphSettings {
	bool shader_inference = false;
	bool use_events = false;
};

struct ResourceBinding {
	vk::Buffer* buf = nullptr;
	vk::Texture* tex = nullptr;
	VkSampler sampler = nullptr;
	bool read = false;
	bool write = false;
	bool active = false;

	ResourceBinding(vk::Buffer* buf) : buf(buf) {}
	ResourceBinding(vk::Texture* tex) : tex(tex) {}
	ResourceBinding(vk::Texture* tex, VkSampler sampler) : tex(tex), sampler(sampler) {}
	inline void replace(const ResourceBinding& binding) {
		if (binding.buf) {
			buf = binding.buf;
		} else {
			tex = binding.tex;
		}
	}

	inline void replace(vk::Texture* tex_, VkSampler sampler_) {
		tex = tex_;
		sampler = sampler_;
	}

	inline vk::DescriptorInfo get_descriptor_info() {
		if (tex) {
			if (sampler) {
				return vk::DescriptorInfo(vk::texture_descriptor(tex, sampler));
			}
			return vk::DescriptorInfo(vk::texture_descriptor(tex));
		}
		return vk::DescriptorInfo(vk::buffer_descriptor(buf));
	}
};

struct Resource {
	vk::Buffer* buf = nullptr;
	vk::Texture* tex = nullptr;
	Resource(vk::Buffer* buf) : buf(buf) {}
	Resource(vk::Texture* tex) : tex(tex) {}
};

struct BufferSyncDescriptor {
	// Read-after-write is the default dependency implicitly
	VkAccessFlags src_access_flags = VK_ACCESS_SHADER_WRITE_BIT;
	VkAccessFlags dst_access_flags = VK_ACCESS_SHADER_READ_BIT;
	u32 opposing_pass_idx;
	VkEvent event = nullptr;
};

struct ImageSyncDescriptor {
	VkImageLayout old_layout;
	VkImageLayout new_layout;
	u32 opposing_pass_idx;
	VkImageAspectFlags image_aspect;
	VkEvent event = nullptr;
};
}  // namespace lm

namespace vk {
enum class PassType { Compute, RT, Graphics };
struct ShaderMacro {
	ShaderMacro(const lm::String& name, i32 val, bool visible)
		: name(name), val(val), has_val(true), visible(visible) {}
	ShaderMacro(const lm::String& name, i32 val) : name(name), val(val), has_val(true) {}
	ShaderMacro(const lm::String& name, bool enable) {
		if (enable) {
			this->name = name;
		}
	}
	ShaderMacro(const lm::String& name) : name(name) {}
	lm::String name;
	i32 val = 0;
	bool has_val = false;
	bool visible = true;
};
struct GraphicsPassSettings {
	std::vector<vk::Shader> shaders;
	lm::FixedArray<ShaderMacro> macros;
	u32 width;
	u32 height;
	VkClearValue clear_color;
	VkClearValue clear_depth_stencil;
	VkCullModeFlags cull_mode = VK_CULL_MODE_FRONT_BIT;
	std::vector<vk::Buffer*> vertex_buffers = {};
	vk::Buffer* index_buffer = nullptr;
	lm::SpecializationConstantArray specialization_data;
	std::vector<bool> blend_enables = {};
	VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	VkIndexType index_type = VK_INDEX_TYPE_UINT32;
	f32 line_width = 1.0;
	std::vector<vk::Texture*> color_outputs = {};
	vk::Texture* depth_output = nullptr;
	std::function<void(VkCommandBuffer cmd, const lm::RenderPass& pass)> pass_func;
	PassType type = PassType::Graphics;
};

struct RTPassSettings {
	std::vector<vk::Shader> shaders;
	lm::FixedArray<ShaderMacro> macros;
	u32 recursion_depth = 1;
	lm::SpecializationConstantArray specialization_data;
	lm::dim3 dims;
	std::function<void(VkCommandBuffer cmd, const lm::RenderPass& pass)> pass_func;
	PassType type = PassType::RT;
};

struct ComputePassSettings {
	vk::Shader shader;
	lm::FixedArray<ShaderMacro> macros;
	lm::SpecializationConstantArray specialization_data;
	lm::dim3 dims;
	std::function<void(VkCommandBuffer cmd, const lm::RenderPass& pass)> pass_func;
	PassType type = PassType::Compute;
};

}  // namespace vk
