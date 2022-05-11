#pragma once
#include "LumenPCH.h"
#include "Framework/Event.h"
#include "Framework/Shader.h"
#include "Framework/Buffer.h"
#include "Framework/SBTWrapper.h"
struct Pipeline;

struct PipelineTrace {
	VkPipeline handle;
	Pipeline* ref;
};

struct GraphicsPipelineSettings {
	std::vector<VkVertexInputBindingDescription> binding_desc;
	std::vector<VkVertexInputAttributeDescription> attribute_desc;
	std::vector<VkDynamicState> dynamic_state_enables;
	std::vector<Shader> shaders;
	std::string name = "";
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkRenderPass render_pass = VK_NULL_HANDLE;
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
	VkCullModeFlags cull_mode = VK_CULL_MODE_FRONT_BIT;
	VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	float line_width = 1.0;
	bool blend_enable = false;
	bool enable_tracking = true;
};

struct RTPipelineSettings {
	std::vector<VkPipelineShaderStageCreateInfo> stages;
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
	std::vector<VkDescriptorSetLayout> desc_layouts;
	std::vector<VkPushConstantRange> push_consts;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props;
	uint32_t max_recursion_depth = 1;
	VulkanContext* ctx;
	std::string name = "";
	std::vector<Shader> shaders;
};

struct ComputePipelineSettings {
	Shader shader;
	uint32_t desc_set_layout_cnt;
	VkDescriptorSetLayout* desc_set_layouts;
	std::vector<uint32_t> specialization_data = {};
	uint32_t push_const_size = 0;
	std::string name = "";
};

struct Pipeline {
public:
	enum class PipelineType {
		GFX = 0,
		RT = 1,
		COMPUTE = 2
	};
	Pipeline(const VkDevice& device);
	void reload();
	void cleanup();
	void create_gfx_pipeline(const GraphicsPipelineSettings&);
	void create_rt_pipeline(RTPipelineSettings&, const std::vector<uint32_t> specialization_data = {});
	void create_compute_pipeline(const ComputePipelineSettings& settings = {});
	const std::array<VkStridedDeviceAddressRegionKHR, 4> get_rt_regions();
	void track_for_changes();
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

	struct {
		uint32_t max_recursion_depth;
		std::vector<VkPipelineShaderStageCreateInfo> stages;
		std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
		VkRayTracingPipelineCreateInfoKHR pipeline_CI = { VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	} rt_cis;

	struct {
		VkSpecializationInfo specialization_info = {};
		VkPipelineShaderStageCreateInfo shader_stage_ci = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
		VkComputePipelineCreateInfo pipeline_CI = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	} compute_cis;


	GraphicsPipelineSettings gfx_settings;
	RTPipelineSettings rt_settings;
	ComputePipelineSettings compute_settings;
	VkDevice device = VK_NULL_HANDLE;
	VkPipeline handle = VK_NULL_HANDLE;
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	SBTWrapper sbt_wrapper;
	SBTWrapper sbt_wrapper_tmp;
	PipelineType type;

	bool running = true;

protected:
	void recompile_pipeline(PipelineType type = PipelineType::GFX);

private:
	bool tracking_stopped = true;
	std::mutex mut;
	std::condition_variable cv;
};
