#include "lmhpch.h"
#include "PipelineDefault.h"

DefaultPipeline::DefaultPipeline(const VkDevice& device,
	const VkPipelineVertexInputStateCreateInfo& vertex_input_state_CI,
	std::vector<Shader>& arg_shaders,
	const std::vector<VkDynamicState>& dynamic_state_enables,
	const VkRenderPass& render_pass,
	const VkPipelineLayout& arg_pipeline_layout)
	: Pipeline(device, arg_shaders), render_pass(render_pass) {
	// Default pipeline setup
	input_asssembly_CI =
		vks::pipeline_vertex_input_assembly_state_CI(
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			VK_NULL_HANDLE,
			VK_FALSE
		);


	viewport_state = vks::pipeline_viewport_state_CI(1, 1, 0);

	rasterizer = vks::pipeline_rasterization_state_CI(
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_BACK_BIT,
		VK_FRONT_FACE_COUNTER_CLOCKWISE
	);
	rasterizer.lineWidth = 1;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.depthBiasEnable = VK_FALSE;


	multisampling = vks::pipeline_multisample_state_CI(
		VK_SAMPLE_COUNT_1_BIT
	);
	multisampling.sampleShadingEnable = VK_FALSE;

	color_blend_attachment = vks::pipeline_color_blend_attachment_state(
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT,
		VK_FALSE
	);

	color_blend = vks::pipeline_color_blend_state_CI(
		1,
		&color_blend_attachment
	);
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.logicOp = VK_LOGIC_OP_COPY;
	color_blend.blendConstants[0] = 0.0f;
	color_blend.blendConstants[1] = 0.0f;
	color_blend.blendConstants[2] = 0.0f;
	color_blend.blendConstants[3] = 0.0f;
	if (arg_pipeline_layout == VK_NULL_HANDLE) {
		pipeline_layout_CI = vks::pipeline_layout_CI((uint32_t)0);
		pipeline_layout_CI.pushConstantRangeCount = 0;
		if (vkCreatePipelineLayout(device, &pipeline_layout_CI, nullptr, &pipeline_layout) != VK_SUCCESS) {
			LUMEN_ERROR("Failed to create pipeline layout!");
		}

	} else {
		pipeline_layout = arg_pipeline_layout;
	}

	dynamic_state_CI =
		vks::pipeline_dynamic_state_CI(
			dynamic_state_enables.data(),
			static_cast<uint32_t>(dynamic_state_enables.size()),
			static_cast<uint32_t>(0));

	pipeline_CI = {};
	pipeline_CI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_CI.stageCount = 2;
	pipeline_CI.pVertexInputState = &vertex_input_state_CI;
	pipeline_CI.pInputAssemblyState = &input_asssembly_CI;
	pipeline_CI.pViewportState = &viewport_state;
	pipeline_CI.pRasterizationState = &rasterizer;
	pipeline_CI.pMultisampleState = &multisampling;
	pipeline_CI.pColorBlendState = &color_blend;
	pipeline_CI.pDynamicState = &dynamic_state_CI;
	pipeline_CI.layout = pipeline_layout;
	// Currently a class field

	if (render_pass == VK_NULL_HANDLE) {
		LUMEN_ERROR("Render pass cannot be null");
	}

	pipeline_CI.renderPass = render_pass;
	pipeline_CI.subpass = 0;
	pipeline_CI.basePipelineHandle = VK_NULL_HANDLE;

	create_pipeline_with_shaders(pipeline_CI);

}

void DefaultPipeline::create_pipeline_with_shaders(VkGraphicsPipelineCreateInfo& ci) {
	VkShaderModule vert_shader = shaders[0].create_vk_shader_module(device);
	VkShaderModule frag_shader = shaders[1].create_vk_shader_module(device);

	vert_shader_CI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_shader_CI.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_shader_CI.module = vert_shader;
	vert_shader_CI.pName = "main";

	frag_shader_CI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_shader_CI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_shader_CI.module = frag_shader;
	frag_shader_CI.pName = "main";

	VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_CI, frag_shader_CI };

	ci.pStages = shader_stages;

	ci.flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_CI, nullptr, &handle) != VK_SUCCESS) {
		LUMEN_ERROR("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(device, frag_shader, nullptr);
	vkDestroyShaderModule(device, vert_shader, nullptr);

}