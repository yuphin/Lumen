#pragma once
#include "Shader.h"
#include "Buffer.h"
#include "Texture.h"
#include "Framework/Base/String.h"
#include "Framework/Base/OS.h"
#include "Framework/Base/Memory.h"
#include "Framework/Base/HashMap.h"
#include "Framework/Base/SmallArray.h"
#include "VkUtils.h"

namespace lm {
struct RenderPass;

static constexpr u64 MAX_SPEC_CONSTANTS = 8;
static constexpr u64 MAX_SHADER_MACROS = 32;
constexpr u64 MAX_VERTEX_BUFFERS = 4;

using SpecializationConstantArray = SmallArray<u32, MAX_SPEC_CONSTANTS>;
using PassFunc = void(*)(VkCommandBuffer cmd, const lm::RenderPass& pass);

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

	ResourceBinding() = default;
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
	VkPipelineStageFlags src_stage;
	VkPipelineStageFlags dst_stage;
	u32 opposing_pass_idx;
	bool event_eligible = true;
	VkEvent event = nullptr;
};

struct ImageSyncDescriptor {
	VkImageLayout old_layout;
	VkImageLayout new_layout;
	VkAccessFlags src_access_flags;
	VkAccessFlags dst_access_flags;
	VkPipelineStageFlags src_stage;
	VkPipelineStageFlags dst_stage;
	VkImageAspectFlags image_aspect;
	u32 opposing_pass_idx;
	bool event_eligible = true;
	VkEvent event = nullptr;
};
}  // namespace lm

namespace vk {
enum class PassType { Compute, RT, Graphics };
struct ShaderMacro {
	ShaderMacro() = default;
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

using ShaderMacroArray = lm::SmallArray<ShaderMacro, lm::MAX_SHADER_MACROS>;

struct GraphicsPassSettings {
	lm::SmallArray<vk::Shader, vk::MAX_SHADERS_PER_PASS> shaders;
	ShaderMacroArray macros;
	u32 width;
	u32 height;
	VkClearValue clear_color;
	VkClearValue clear_depth_stencil;
	VkCullModeFlags cull_mode = VK_CULL_MODE_FRONT_BIT;
	lm::SmallArray<vk::Buffer*, lm::MAX_VERTEX_BUFFERS> vertex_buffers;
	vk::Buffer* index_buffer = nullptr;
	lm::SpecializationConstantArray specialization_data;
	lm::SmallArray<bool, MAX_COLOR_ATTACHMENTS> blend_enables;
	VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	VkIndexType index_type = VK_INDEX_TYPE_UINT32;
	f32 line_width = 1.0;
	lm::SmallArray<vk::Texture*, MAX_COLOR_ATTACHMENTS> color_outputs = {};
	vk::Texture* depth_output = nullptr;
	lm::PassFunc pass_func;

	PassType type = PassType::Graphics;
};

struct RTPassSettings {
	lm::SmallArray<vk::Shader, vk::MAX_SHADERS_PER_PASS> shaders;
	ShaderMacroArray macros;
	u32 recursion_depth = 1;
	lm::SpecializationConstantArray specialization_data;
	lm::dim3 dims;
	PassType type = PassType::RT;
	lm::PassFunc pass_func;
};

struct ComputePassSettings {
	vk::Shader shader;
	ShaderMacroArray macros;
	lm::SpecializationConstantArray specialization_data;
	lm::dim3 dims;
	lm::PassFunc pass_func;
	PassType type = PassType::Compute;
};

struct PassSettings {
	////////////////////////////
	// --- Common ---
	lm::SmallArray<vk::Shader, vk::MAX_SHADERS_PER_PASS> shaders = {};
	ShaderMacroArray macros = {};
	lm::SpecializationConstantArray specialization_data = {};
	lm::dim3 dims = {};
	lm::PassFunc pass_func = nullptr;

	////////////////////////////
	// --- Graphics ---
	u32 width = 0;
	u32 height = 0;
	VkClearValue clear_color = {};
	VkClearValue clear_depth_stencil = {};
	VkCullModeFlags cull_mode = VK_CULL_MODE_FRONT_BIT;
	lm::SmallArray<vk::Buffer*, lm::MAX_VERTEX_BUFFERS> vertex_buffers = {};
	vk::Buffer* index_buffer = nullptr;
	lm::SmallArray<bool, MAX_COLOR_ATTACHMENTS> blend_enables = {};
	VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	VkIndexType index_type = VK_INDEX_TYPE_UINT32;
	f32 line_width = 1.0;
	lm::SmallArray<vk::Texture*, MAX_COLOR_ATTACHMENTS> color_outputs = {};
	vk::Texture* depth_output = nullptr;

	////////////////////////////
	// --- RT ---
	u32 recursion_depth = 1;
};

}  // namespace vk
