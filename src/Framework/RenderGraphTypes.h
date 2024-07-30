#pragma once
#include <cstdint>
#include <volk/volk.h>
#include <vector>
#include <functional>
#include "Shader.h"
#include "Buffer.h"
#include "Texture.h"
#include "CommonTypes.h"

namespace lumen {
class RenderPass;

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

struct RenderGraphSettings {
	bool shader_inference = false;
	bool use_events = false;
};


enum class PassType { Compute, RT, Graphics };

struct GraphicsPassSettings {
	std::vector<Shader> shaders;
	const std::vector<ShaderMacro> macros = {};
	uint32_t width;
	uint32_t height;
	VkClearValue clear_color;
	VkClearValue clear_depth_stencil;
	VkCullModeFlags cull_mode = VK_CULL_MODE_FRONT_BIT;
	std::vector<Buffer*> vertex_buffers = {};
	Buffer* index_buffer = nullptr;
	std::vector<uint32_t> specialization_data = {};
	std::vector<bool> blend_enables = {};
	VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	VkIndexType index_type = VK_INDEX_TYPE_UINT32;
	float line_width = 1.0;
	std::vector<Texture2D*> color_outputs = {};
	Texture2D* depth_output = nullptr;
	std::function<void(VkCommandBuffer cmd, const RenderPass& pass)> pass_func;
	PassType type = PassType::Graphics; 
};

struct RTPassSettings {
	std::vector<Shader> shaders;
	std::vector<ShaderMacro> macros = {};
	uint32_t recursion_depth = 1;
	std::vector<uint32_t> specialization_data = {};
	dim3 dims;
	std::function<void(VkCommandBuffer cmd, const RenderPass& pass)> pass_func;
	PassType type = PassType::RT;
};

struct ComputePassSettings {
	Shader shader;
	std::vector<ShaderMacro> macros = {};
	std::vector<uint32_t> specialization_data = {};
	dim3 dims;
	std::function<void(VkCommandBuffer cmd, const RenderPass& pass)> pass_func;
	PassType type = PassType::Compute;
};


struct ResourceBinding {
	Buffer* buf = nullptr;
	Texture2D* tex = nullptr;
	VkSampler sampler = nullptr;
	bool read = false;
	bool write = false;
	bool active = false;

	ResourceBinding(Buffer& buf) : buf(&buf) {}
	ResourceBinding(Texture2D& tex) : tex(&tex) {}
	ResourceBinding(Texture2D& tex, VkSampler sampler) : tex(&tex), sampler(sampler) {}
	inline void replace(const ResourceBinding& binding) {
		if (binding.buf) {
			this->buf = binding.buf;
		} else {
			this->tex = binding.tex;
		}
	}

	inline void replace(Texture2D& tex, VkSampler sampler) {
		this->tex = &tex;
		this->sampler = sampler;
	}

	inline vk::DescriptorInfo get_descriptor_info() {
		if (tex) {
			if (sampler) {
				return vk::DescriptorInfo(tex->descriptor(sampler));
			}
			return vk::DescriptorInfo(tex->descriptor());
		}
		return vk::DescriptorInfo(buf->descriptor);
	}
};

struct Resource {
	Buffer* buf = nullptr;
	Texture2D* tex = nullptr;
	Resource(Buffer& buf) : buf(&buf) {}
	Resource(Texture2D& tex) : tex(&tex) {}
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
