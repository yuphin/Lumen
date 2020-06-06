#include "Lumen.h"


Lumen::Lumen(int width, int height, bool fullscreen, bool debug) :
	VKBase(width, height, fullscreen, debug) {}




void Lumen::create_render_pass() {
	// Simple render pass for a triangle 
	VkAttachmentDescription color_attachment{};
	color_attachment.format = swapchain_image_format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref{};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_CI{};
	render_pass_CI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_CI.attachmentCount = 1;
	render_pass_CI.pAttachments = &color_attachment;
	render_pass_CI.subpassCount = 1;
	render_pass_CI.pSubpasses = &subpass;
	render_pass_CI.dependencyCount = 1;
	render_pass_CI.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &render_pass_CI, nullptr, &render_pass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}
}

void Lumen::create_gfx_pipeline() {
	const std::string vert_shader_name = "triangle.vert.spv";
	const std::string frag_shader_name = "triangle.frag.spv";
	auto vert_shader_code = read_file("src/shaders/spv/" + vert_shader_name);
	auto frag_shader_code = read_file("src/shaders/spv/" + frag_shader_name);

	VkShaderModule vert_shader = create_shader(vert_shader_code);
	VkShaderModule frag_shader = create_shader(frag_shader_code);


	VkPipelineShaderStageCreateInfo vert_shader_CI{};
	vert_shader_CI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_shader_CI.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_shader_CI.module = vert_shader;
	vert_shader_CI.pName = "main";

	VkPipelineShaderStageCreateInfo frag_shader_CI{};
	frag_shader_CI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_shader_CI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_shader_CI.module = frag_shader;
	frag_shader_CI.pName = "main";

	VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_CI, frag_shader_CI };

	VkPipelineVertexInputStateCreateInfo vertex_input_info = vks::pipeline_vertex_input_state_CI();
	vertex_input_info.vertexBindingDescriptionCount = 0;
	vertex_input_info.vertexAttributeDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo input_asssembly_CI =
		vks::pipeline_vertex_input_assembly_state_CI(
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			VK_NULL_HANDLE,
			VK_FALSE
		);

	VkViewport viewport = vks::viewport(
		static_cast<float>(swapchain_extent.width), 
		static_cast<float>(swapchain_extent.height), 
		0.0, 
		1.0
	);
	VkRect2D scissor = vks::rect2D(swapchain_extent.width,
		swapchain_extent.height, 0, 0
	);

	VkPipelineViewportStateCreateInfo viewport_state = vks::pipeline_viewport_state_CI(
		1, &viewport, 1, &scissor
	);

	VkPipelineRasterizationStateCreateInfo rasterizer = vks::pipeline_rasterization_state_CI(
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_BACK_BIT,
		VK_FRONT_FACE_CLOCKWISE
	);
	rasterizer.lineWidth = 1;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.depthBiasEnable = VK_FALSE;


	VkPipelineMultisampleStateCreateInfo multisampling = vks::pipeline_multisample_state_CI(
		VK_SAMPLE_COUNT_1_BIT
	);
	multisampling.sampleShadingEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState color_blend_attachment = vks::pipeline_color_blend_attachment_state(
		VK_COLOR_COMPONENT_R_BIT | 
		VK_COLOR_COMPONENT_G_BIT | 
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT,
		VK_FALSE
	);

	VkPipelineColorBlendStateCreateInfo color_blend = vks::pipeline_color_blend_state_CI(
		1,
		&color_blend_attachment
	);
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.logicOp = VK_LOGIC_OP_COPY;
	color_blend.blendConstants[0] = 0.0f;
	color_blend.blendConstants[1] = 0.0f;
	color_blend.blendConstants[2] = 0.0f;
	color_blend.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipeline_CI = vks::pipeline_layout_CI((uint32_t)0);
	pipeline_CI.pushConstantRangeCount = 0;

	if (vkCreatePipelineLayout(device, &pipeline_CI, nullptr, &pipeline_layout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shader_stages;
	pipelineInfo.pVertexInputState = &vertex_input_info;
	pipelineInfo.pInputAssemblyState = &input_asssembly_CI;
	pipelineInfo.pViewportState = &viewport_state;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &color_blend;
	pipelineInfo.layout = pipeline_layout;
	// Currently a class field
	pipelineInfo.renderPass = render_pass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gfx_pipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(device, frag_shader, nullptr);
	vkDestroyShaderModule(device, vert_shader, nullptr);
}

void Lumen::create_framebuffers() {
	swapchain_framebuffers.resize(swapchain_image_views.size());

	for (size_t i = 0; i < swapchain_image_views.size(); i++) {
		VkImageView attachments[] = {
			swapchain_image_views[i]
		};

		VkFramebufferCreateInfo frame_buffer_info{};
		frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frame_buffer_info.renderPass = render_pass;
		frame_buffer_info.attachmentCount = 1;
		frame_buffer_info.pAttachments = attachments;
		frame_buffer_info.width = swapchain_extent.width;
		frame_buffer_info.height = swapchain_extent.height;
		frame_buffer_info.layers = 1;

		if (vkCreateFramebuffer(device, &frame_buffer_info, nullptr, &swapchain_framebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to create framebuffer");
		}
	}

}

void Lumen::create_command_buffers() {
	command_buffers.resize(swapchain_framebuffers.size());

	VkCommandBufferAllocateInfo alloc_info = vks::command_buffer_allocate_info(
		command_pool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		(uint32_t)command_buffers.size()
	);

	if (vkAllocateCommandBuffers(device, &alloc_info, command_buffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}

	for (size_t i = 0; i < command_buffers.size(); i++) {
		VkCommandBufferBeginInfo begin_info = vks::command_buffer_begin_info();
		if (vkBeginCommandBuffer(command_buffers[i], &begin_info) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo render_pass_info = vks::render_pass_begin_info();
		render_pass_info.renderPass = render_pass;
		render_pass_info.framebuffer = swapchain_framebuffers[i];
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = swapchain_extent;

		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		render_pass_info.clearValueCount = 1;
		render_pass_info.pClearValues = &clearColor;

		vkCmdBeginRenderPass(command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, gfx_pipeline);

		vkCmdDraw(command_buffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(command_buffers[i]);

		if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("Failed to record command buffer");
		}
	}


}

void Lumen::run() {
	VKBase::init();
	VKBase::render_loop();
	VKBase::cleanup();
}



