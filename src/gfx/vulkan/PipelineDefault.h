#pragma once
#include "Pipeline.h"
#include "lmhpch.h"
struct DefaultPipeline : Pipeline {
	VkRenderPass render_pass;

	DefaultPipeline(const VkDevice& device,
		const VkPipelineVertexInputStateCreateInfo& vertex_input_state_CI,
		std::vector<Shader>& arg_shaders,
		const std::vector<VkDynamicState>& dynamic_state_enables,
		const VkRenderPass& render_pass = VK_NULL_HANDLE,
	    const VkPipelineLayout& pipeline_layout = VK_NULL_HANDLE);
	void create_pipeline_with_shaders(VkGraphicsPipelineCreateInfo& ci) override;

};
