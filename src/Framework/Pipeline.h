#pragma once
#include "LumenPCH.h"
#include "Framework/Event.h"
#include "Framework/Shader.h"
#include "Framework/Buffer.h"
#include "Framework/Texture.h"
#include "Framework/SBTWrapper.h"
#include "Framework/RenderGraphTypes.h"
struct Pipeline;

struct PipelineTrace {
	VkPipeline handle;
	Pipeline* ref;
};

//struct GraphicsPipelineSettings {
//	std::vector<Shader> shaders;
//	VkCullModeFlags cull_mode = VK_CULL_MODE_FRONT_BIT;
//	std::vector<Buffer*> vertex_buffers = {};
//	Buffer* index_buffer = nullptr;
//	std::vector<uint32_t> specialization_data = {};
//	std::vector<bool> blend_enables = {};
//	VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
//	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
//	VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
//	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
//	VkIndexType index_type = VK_INDEX_TYPE_UINT32;
//	float line_width = 1.0;
//};
//
//struct RTPipelineSettings {
//	std::vector<Shader> shaders;
//	uint32_t recursion_depth = 1;
//	std::vector<uint32_t> specialization_data = {};
//};
//
//
//struct ComputePipelineSettings {
//	Shader shader;
//	std::vector<uint32_t> specialization_data = {};
//};



struct Pipeline {
public:
	enum class PipelineType {
		GFX = 0,
		RT = 1,
		COMPUTE = 2
	};
	Pipeline(VulkanContext* ctx, size_t pass_idx, const std::string& name);
	void reload();
	void cleanup();
	void create_gfx_pipeline(const GraphicsPassSettings& settings,
							 const std::vector<uint32_t>& descriptor_counts,
							 std::vector<Texture2D*> color_outputs,
							 Texture2D* depth_output);
	void create_rt_pipeline(const RTPassSettings& settings, const std::vector<uint32_t>& descriptor_counts);
	void create_compute_pipeline(const ComputePassSettings& settings, const std::vector<uint32_t>& descriptor_counts);
	const std::array<VkStridedDeviceAddressRegionKHR, 4> get_rt_regions();
	//void track_for_changes();
	// Manually called after the pipeline is reloaded
	void refresh();
	struct {
		VkPipelineShaderStageCreateInfo vert_shader_CI;
		VkPipelineShaderStageCreateInfo frag_shader_CI;
		VkPipelineInputAssemblyStateCreateInfo input_asssembly_CI;
		VkPipelineViewportStateCreateInfo viewport_state;
		VkPipelineRasterizationStateCreateInfo rasterizer;
		VkPipelineMultisampleStateCreateInfo multisampling;
		VkPipelineColorBlendAttachmentState color_blend_attachment;
		VkPipelineColorBlendStateCreateInfo color_blend;
		VkPipelineLayoutCreateInfo pipeline_layout_CI;
		VkPipelineDynamicStateCreateInfo dynamic_state_CI;
		VkGraphicsPipelineCreateInfo pipeline_CI;
	} gfx_cis;
	std::unordered_map<std::string, std::filesystem::file_time_type> paths;

	//GraphicsPipelineSettings gfx_settings;
	//RTPipelineSettings rt_settings;
	//ComputePipelineSettings compute_settings;
	VkPipeline handle = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout set_layout, tlas_layout;
	SBTWrapper sbt_wrapper;
	PipelineType type;
	size_t pass_idx;
	bool running = true;
	VkDescriptorUpdateTemplate update_template;
	VkShaderStageFlags pc_stages = 0;
	std::string name;
	uint32_t push_constant_size = 0;

private:
	void create_set_layout(const std::vector<Shader>& shaders, const std::vector<uint32_t>& descriptor_counts);
	void create_pipeline_layout(const std::vector<Shader>& shaders, const std::vector<uint32_t> push_const_sizes);
	void create_update_template(const std::vector<Shader>& shaders, const std::vector<uint32_t>& descriptor_counts);
	bool tracking_stopped = true;
	std::mutex mut;
	std::condition_variable cv;
	uint32_t binding_mask;
	VkDescriptorType descriptor_types[32] = {};
	VulkanContext* ctx;
};
