#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include "Framework/Shader.h"
#include "Framework/Buffer.h"
#include "Framework/Texture.h"
#include "CommonTypes.h"

class RenderGraph;
class RenderPass;

struct RenderGraphSettings {
	bool shader_inference = false;
};

struct GraphicsPassSettings {
	uint32_t width;
	uint32_t height;
	VkClearValue clear_color;
	VkClearValue clear_depth_stencil;
	std::vector<Shader> shaders;
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
};

struct RTPassSettings {
	std::vector<Shader> shaders;
	uint32_t recursion_depth = 1;
	std::vector<uint32_t> specialization_data = {};
	dim3 dims;
	VkAccelerationStructureKHR accel;
	std::function<void(VkCommandBuffer cmd, const RenderPass& pass)> pass_func;
};

struct ComputePassSettings {
	Shader shader;
	std::vector<uint32_t> specialization_data = {};
	dim3 dims;
	std::function<void(VkCommandBuffer cmd, const RenderPass& pass)> pass_func;
};

enum class PassType { Compute, RT, Graphics };

struct ResourceBinding {
	struct ResourceStatus {
		bool read = false;
		bool write = false;
	};
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

	inline DescriptorInfo get_descriptor_info() {
		if (tex) {
			if (sampler) {
				return DescriptorInfo(tex->descriptor(sampler));
			}
			return DescriptorInfo(tex->descriptor());
		}
		return DescriptorInfo(buf->descriptor);
	}
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
