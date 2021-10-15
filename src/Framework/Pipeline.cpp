#include "LumenPCH.h"

#include "Pipeline.h"


Pipeline::Pipeline(const VkDevice& device) : device(device) {}

void Pipeline::track_for_changes() {
	for(auto& file : settings.shaders) {
		const std::filesystem::path path = file.filename;
		paths[file.filename] = std::filesystem::last_write_time(path);
	}
	ThreadPool::submit([this] {
		tracking_stopped = false;
		std::chrono::duration<int, std::milli> delay = std::chrono::milliseconds(100);
		while(running) {
			std::this_thread::sleep_for(delay);
			for(auto& file : settings.shaders) {
				const std::filesystem::path path = file.filename;
				auto last_write = std::filesystem::last_write_time(path);
				if(paths.find(file.filename) != paths.end() &&
				   last_write != paths[file.filename]) {
					LUMEN_TRACE("Shader changed: {0}", file.filename);
					paths[file.filename] = last_write;
					if(!file.compile()) {
						pipeline_CI.flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
						pipeline_CI.basePipelineHandle = handle;
						pipeline_CI.basePipelineIndex = -1;
						EventHandler::obsolete_pipelines.push_back(handle);
						this->recompile_pipeline();
						EventHandler::set(LumenEvent::EVENT_SHADER_RELOAD);
					}
				}
			}
		}
		{
			std::lock_guard<std::mutex> lk(mut);
			tracking_stopped = true;
		}
		cv.notify_one();
	});

}

void Pipeline::cleanup() {
	if(handle != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, handle, nullptr);
	}
	running = false;
	std::unique_lock<std::mutex> tracker_lk(mut);
	cv.wait(tracker_lk, [this] {return tracking_stopped; });
}

void Pipeline::create_gfx_pipeline(const GraphicsPipelineSettings& settings) {
	LUMEN_ASSERT(settings.pipeline_layout, "Pipeline layout cannot be null");
	LUMEN_ASSERT(settings.render_pass, "Render pass cannot be null");

	input_asssembly_CI =
		vk::pipeline_vertex_input_assembly_state_CI(
			settings.topology,
			0,
			VK_FALSE
		);
	viewport_state = vk::pipeline_viewport_state_CI(1, 1, 0);
	rasterizer = vk::pipeline_rasterization_state_CI(
		settings.polygon_mode,
		settings.cull_mode,
		settings.front_face
	);
	rasterizer.lineWidth = settings.line_width;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.depthBiasEnable = VK_FALSE;
	multisampling = vk::pipeline_multisample_state_CI(
		settings.sample_count
	);
	multisampling.sampleShadingEnable = VK_FALSE;
	color_blend_attachment = vk::pipeline_color_blend_attachment_state(
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT,
		settings.blend_enable
	);
	color_blend = vk::pipeline_color_blend_state_CI(
		1,
		&color_blend_attachment
	);
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.logicOp = VK_LOGIC_OP_COPY;
	color_blend.blendConstants[0] = 0.0f;
	color_blend.blendConstants[1] = 0.0f;
	color_blend.blendConstants[2] = 0.0f;
	color_blend.blendConstants[3] = 0.0f;
	dynamic_state_CI =
		vk::pipeline_dynamic_state_CI(
			settings.dynamic_state_enables.data(),
			static_cast<uint32_t>(settings.dynamic_state_enables.size()),
			static_cast<uint32_t>(0));

	auto vertex_input_state = vk::pipeline_vertex_input_state_CI();
	vertex_input_state.vertexAttributeDescriptionCount =
		static_cast<uint32_t>(settings.attribute_desc.size());
	vertex_input_state.pVertexAttributeDescriptions = settings.attribute_desc.data();
	vertex_input_state.vertexBindingDescriptionCount =
		static_cast<uint32_t>(settings.binding_desc.size());
	vertex_input_state.pVertexBindingDescriptions = settings.binding_desc.data();
	VkShaderModule vert_shader = settings.shaders[0].create_vk_shader_module(device);
	VkShaderModule frag_shader = settings.shaders[1].create_vk_shader_module(device);


	vert_shader_CI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_shader_CI.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_shader_CI.module = vert_shader;
	vert_shader_CI.pName = "main";
	vert_shader_CI.pNext = nullptr;
	vert_shader_CI.flags = 0;
	vert_shader_CI.pSpecializationInfo = nullptr;

	frag_shader_CI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_shader_CI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_shader_CI.module = frag_shader;
	frag_shader_CI.pName = "main";
	frag_shader_CI.pNext = nullptr;
	frag_shader_CI.flags = 0;
	frag_shader_CI.pSpecializationInfo = nullptr;
	VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_CI, frag_shader_CI };

	auto depth_stencil_state_ci = vk::pipeline_depth_stencil_CI(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	pipeline_CI = vk::graphics_pipeline_CI();
	pipeline_CI.pNext = nullptr;
	pipeline_CI.stageCount = 2;
	pipeline_CI.pStages = shader_stages;
	pipeline_CI.pVertexInputState = &vertex_input_state;
	pipeline_CI.pInputAssemblyState = &input_asssembly_CI;
	pipeline_CI.pViewportState = &viewport_state;
	pipeline_CI.pRasterizationState = &rasterizer;
	pipeline_CI.pMultisampleState = &multisampling;
	pipeline_CI.pColorBlendState = &color_blend;
	pipeline_CI.pDynamicState = &dynamic_state_CI;
	pipeline_CI.layout = settings.pipeline_layout;
	pipeline_CI.renderPass = settings.render_pass;
	pipeline_CI.subpass = 0;
	pipeline_CI.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_CI.pDepthStencilState = &depth_stencil_state_ci;

	this->handle = settings.pipeline;
	this->settings = settings;

	if(settings.enable_tracking) {
		pipeline_CI.flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	}
	vk::check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_CI, nullptr, &handle),
			  "Failed to create pipeline");

	vkDestroyShaderModule(device, frag_shader, nullptr);
	vkDestroyShaderModule(device, vert_shader, nullptr);
}

void Pipeline::create_rt_pipeline(const RTPipelineSettings& settings) {
	// TODO:
	
}

void Pipeline::recompile_pipeline() {
	VkShaderModule vert_shader = settings.shaders[0].create_vk_shader_module(device);
	VkShaderModule frag_shader = settings.shaders[1].create_vk_shader_module(device);
	auto vertex_input_state_CI = vk::pipeline_vertex_input_state_CI();
	vert_shader_CI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_shader_CI.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_shader_CI.module = vert_shader;
	vert_shader_CI.pName = "main";
	vert_shader_CI.pNext = nullptr;
	vert_shader_CI.flags = 0;
	vert_shader_CI.pSpecializationInfo = nullptr;

	frag_shader_CI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_shader_CI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_shader_CI.module = frag_shader;
	frag_shader_CI.pName = "main";
	frag_shader_CI.pNext = nullptr;
	frag_shader_CI.flags = 0;
	frag_shader_CI.pSpecializationInfo = nullptr;
	VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_CI, frag_shader_CI };
	pipeline_CI.pStages = shader_stages;

	vertex_input_state_CI.vertexAttributeDescriptionCount =
		static_cast<uint32_t>(settings.attribute_desc.size());
	vertex_input_state_CI.pVertexAttributeDescriptions = settings.attribute_desc.data();
	vertex_input_state_CI.vertexBindingDescriptionCount =
		static_cast<uint32_t>(settings.binding_desc.size());
	vertex_input_state_CI.pVertexBindingDescriptions = settings.binding_desc.data();
	pipeline_CI.pVertexInputState = &vertex_input_state_CI;
	vk::check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_CI, nullptr, &handle),
			  "Failed to create pipeline");
	vkDestroyShaderModule(device, frag_shader, nullptr);
	vkDestroyShaderModule(device, vert_shader, nullptr);
}
