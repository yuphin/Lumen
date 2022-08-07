#pragma once
#include <cstdint>
#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include "Framework/Shader.h"
#include "Framework/Buffer.h"
#include "Framework/Texture.h"
struct dim3 {
	uint32_t x = 1;
	uint32_t y = 1;
	uint32_t z = 1;
};

class RenderGraph;
class RenderPass;

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
