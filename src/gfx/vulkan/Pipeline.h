#pragma once
#include "lmhpch.h"
#include "gfx/Shader.h"
#include "core/Event.h"
#

struct Pipeline {
	VkPipeline handle;
	VkPipelineLayout pipeline_layout;
	std::vector<Shader> shaders;
	VkDevice device;
	std::thread tracker;
	std::unordered_map<std::string, std::filesystem::file_time_type> paths;
	VkGraphicsPipelineCreateInfo pipeline_CI;
	bool running = true;

	Pipeline(const VkDevice& device, std::vector<Shader>& shaders);	
	
	void cleanup();
	virtual void update_pipeline() = 0;
private:
	void track();

protected:
	VkRenderPass render_pass;

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




};
