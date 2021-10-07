#pragma once
#include "LumenPCH.h"
#include "Framework/Shader.h"
#include "Framework/Event.h"
#include "Framework/Model.h"

struct Pipeline;

struct GraphicsPipelineSettings {
	std::vector<VkVertexInputBindingDescription> binding_desc;
	std::vector<VkVertexInputAttributeDescription> attribute_desc;
	std::vector<VkDynamicState> dynamic_state_enables;
	std::vector<Shader> shaders;
	std::string name = "";
	VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkRenderPass render_pass = VK_NULL_HANDLE;
	std::span<Model*> bound_models = {};
	std::function<void(Pipeline*, const std::span<Model*>&)> custom_func = nullptr;
	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPolygonMode polygon_mode = VK_POLYGON_MODE_FILL;
	VkCullModeFlags cull_mode = VK_CULL_MODE_FRONT_BIT;
	VkFrontFace front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	float line_width = 1.0;
	bool blend_enable = false;
	bool enable_tracking = true;

};

struct Pipeline {
public:
	Pipeline(const VkDevice& device);	
	void cleanup();
	void create_pipeline(const GraphicsPipelineSettings&);
	void track_for_changes();
	std::unordered_map<std::string, std::filesystem::file_time_type> paths;
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
	GraphicsPipelineSettings settings;
	VkDevice device = VK_NULL_HANDLE;
	VkPipeline handle = VK_NULL_HANDLE;
	bool running = true;
private:
	void recompile_pipeline();
	bool tracking_stopped = true;
	std::mutex mut;
	std::condition_variable cv;
};

