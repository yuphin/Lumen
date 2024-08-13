#pragma once
#include <cstdint>
#include <volk/volk.h>
#include <vector>
#include <functional>
#include "Shader.h"
#include "Buffer.h"
#include "Texture.h"
#include "CommonTypes.h"

namespace vk {
enum class PassType { Compute, RT, Graphics };
struct ShaderMacro {
	ShaderMacro(const std::string& name, int val, bool visible)
		: name(name), val(val), has_val(true), visible(visible) {}
	ShaderMacro(const std::string& name, int val) : name(name), val(val), has_val(true) {}
	ShaderMacro(const std::string& name, bool enable) {
		if (enable) {
			this->name = name;
		}
	}
	ShaderMacro(const std::string& name) : name(name) {}
	std::string name = "";
	int val = 0;
	bool has_val = false;
	bool visible = true;
};
struct GraphicsPassSettings {
	std::vector<vk::Shader> shaders;
	const std::vector<ShaderMacro> macros = {};
	uint32_t width;
	uint32_t height;
	VkClearValue clear_color;
	VkClearValue clear_depth_stencil;
	VkCullModeFlags cull_mode = VK_CULL_MODE_FRONT_BIT;
	std::vector<vk::Buffer*> vertex_buffers = {};
	vk::Buffer* index_buffer = nullptr;
	std::vector<uint32_t> specialization_data = {};
	std::vector<bool> blend_enables = {};
	VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	VkIndexType index_type = VK_INDEX_TYPE_UINT32;
	float line_width = 1.0;
	std::vector<vk::Texture*> color_outputs = {};
	vk::Texture* depth_output = nullptr;
	std::function<void(VkCommandBuffer cmd, const lumen::RenderPass& pass)> pass_func;
	PassType type = PassType::Graphics;
};

struct RTPassSettings {
	std::vector<vk::Shader> shaders;
	std::vector<ShaderMacro> macros = {};
	uint32_t recursion_depth = 1;
	std::vector<uint32_t> specialization_data = {};
	lumen::dim3 dims;
	std::function<void(VkCommandBuffer cmd, const lumen::RenderPass& pass)> pass_func;
	PassType type = PassType::RT;
};

struct ComputePassSettings {
	vk::Shader shader;
	std::vector<ShaderMacro> macros = {};
	std::vector<uint32_t> specialization_data = {};
	lumen::dim3 dims;
	std::function<void(VkCommandBuffer cmd, const lumen::RenderPass& pass)> pass_func;
	PassType type = PassType::Compute;
};

}  // namespace vk
namespace lumen {
class RenderPass;

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
			this->buf = binding.buf;
		} else {
			this->tex = binding.tex;
		}
	}

	inline void replace(vk::Texture* tex, VkSampler sampler) {
		this->tex = tex;
		this->sampler = sampler;
	}

	inline vk::DescriptorInfo get_descriptor_info() {
		if (tex) {
			if (sampler) {
				return vk::DescriptorInfo(vk::get_texture_descriptor(tex, sampler));
			}
			return vk::DescriptorInfo(vk::get_texture_descriptor(tex));
		}
		return vk::DescriptorInfo(vk::get_buffer_descriptor(buf));
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
	uint32_t opposing_pass_idx;
	VkEvent event = nullptr;
};

struct ImageSyncDescriptor {
	VkImageLayout old_layout;
	VkImageLayout new_layout;
	uint32_t opposing_pass_idx;
	VkImageAspectFlags image_aspect;
	VkEvent event = nullptr;
};
}  // namespace lumen
