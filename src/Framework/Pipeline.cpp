#include "LumenPCH.h"

#include "Pipeline.h"

Pipeline::Pipeline(const VkDevice& device) : device(device) {}

void Pipeline::track_for_changes() {
	for (auto& file : settings.shaders) {
		const std::filesystem::path path = file.filename;
		paths[file.filename] = std::filesystem::last_write_time(path);
	}
	ThreadPool::submit([this] {
		tracking_stopped = false;
		std::chrono::duration<int, std::milli> delay =
			std::chrono::milliseconds(100);
		while (running) {
			std::this_thread::sleep_for(delay);
			for (auto& file : settings.shaders) {
				const std::filesystem::path path = file.filename;
				auto last_write = std::filesystem::last_write_time(path);
				if (paths.find(file.filename) != paths.end() &&
					last_write != paths[file.filename]) {
					LUMEN_TRACE("Shader changed: {0}", file.filename);
					paths[file.filename] = last_write;
					if (!file.compile()) {
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


void Pipeline::create_gfx_pipeline(const GraphicsPipelineSettings& settings) {
	LUMEN_ASSERT(settings.pipeline_layout, "Pipeline layout cannot be null");
	LUMEN_ASSERT(settings.render_pass, "Render pass cannot be null");

	input_asssembly_CI = vk::pipeline_vertex_input_assembly_state_CI(
		settings.topology, 0, VK_FALSE);
	viewport_state = vk::pipeline_viewport_state_CI(1, 1, 0);
	rasterizer = vk::pipeline_rasterization_state_CI(
		settings.polygon_mode, settings.cull_mode, settings.front_face);
	rasterizer.lineWidth = settings.line_width;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.depthBiasEnable = VK_FALSE;
	multisampling = vk::pipeline_multisample_state_CI(settings.sample_count);
	multisampling.sampleShadingEnable = VK_FALSE;
	color_blend_attachment = vk::pipeline_color_blend_attachment_state(
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		settings.blend_enable);
	color_blend = vk::pipeline_color_blend_state_CI(1, &color_blend_attachment);
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.logicOp = VK_LOGIC_OP_COPY;
	color_blend.blendConstants[0] = 0.0f;
	color_blend.blendConstants[1] = 0.0f;
	color_blend.blendConstants[2] = 0.0f;
	color_blend.blendConstants[3] = 0.0f;
	dynamic_state_CI = vk::pipeline_dynamic_state_CI(
		settings.dynamic_state_enables.data(),
		static_cast<uint32_t>(settings.dynamic_state_enables.size()),
		static_cast<uint32_t>(0));

	auto vertex_input_state = vk::pipeline_vertex_input_state_CI();
	vertex_input_state.vertexAttributeDescriptionCount =
		static_cast<uint32_t>(settings.attribute_desc.size());
	vertex_input_state.pVertexAttributeDescriptions =
		settings.attribute_desc.data();
	vertex_input_state.vertexBindingDescriptionCount =
		static_cast<uint32_t>(settings.binding_desc.size());
	vertex_input_state.pVertexBindingDescriptions =
		settings.binding_desc.data();
	VkShaderModule vert_shader =
		settings.shaders[0].create_vk_shader_module(device);
	VkShaderModule frag_shader =
		settings.shaders[1].create_vk_shader_module(device);

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
	VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_CI,
													   frag_shader_CI };

	auto depth_stencil_state_ci =
		vk::pipeline_depth_stencil_CI(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

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

	if (settings.enable_tracking) {
		pipeline_CI.flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	}
	vk::check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_CI,
		nullptr, &handle),
		"Failed to create pipeline");

	vkDestroyShaderModule(device, frag_shader, nullptr);
	vkDestroyShaderModule(device, vert_shader, nullptr);
}

void Pipeline::create_rt_pipeline(RTPipelineSettings& settings,
	const std::vector<uint32_t> specialization_data) {
	VkPipelineLayoutCreateInfo pipeline_layout_create_info{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipeline_layout_create_info.setLayoutCount = settings.desc_layouts.size();
	pipeline_layout_create_info.pSetLayouts = settings.desc_layouts.data();

	auto pcr_cnt = settings.push_consts.size();
	if (pcr_cnt) {
		pipeline_layout_create_info.pushConstantRangeCount = pcr_cnt;
		pipeline_layout_create_info.pPushConstantRanges = settings.push_consts.data();
	}
	std::vector<VkSpecializationMapEntry> entries(specialization_data.size());
	for (int i = 0; i < entries.size(); i++) {
		entries[i].constantID = i;
		entries[i].size = sizeof(uint32_t);
		entries[i].offset = i * sizeof(uint32_t);
	}
	VkSpecializationInfo specialization_info{};
	specialization_info.dataSize = specialization_data.size() * sizeof(uint32_t);
	specialization_info.mapEntryCount = (uint32_t)specialization_data.size();
	specialization_info.pMapEntries = entries.data();
	specialization_info.pData = specialization_data.data();
	if (!specialization_data.empty()) {
		for (auto& stage : settings.stages) {
			stage.pSpecializationInfo = &specialization_info;
		}
	}

	vkCreatePipelineLayout(device, &pipeline_layout_create_info,
		nullptr, &pipeline_layout);
	VkRayTracingPipelineCreateInfoKHR pipeline_create_info{
	VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	pipeline_create_info.stageCount =
		static_cast<uint32_t>(settings.stages.size());
	pipeline_create_info.pStages = settings.stages.data();
	pipeline_create_info.groupCount = static_cast<uint32_t>(settings.groups.size());
	pipeline_create_info.pGroups = settings.groups.data();
	pipeline_create_info.maxPipelineRayRecursionDepth = settings.max_recursion_depth;
	pipeline_create_info.layout = pipeline_layout;
	vkCreateRayTracingPipelinesKHR(device, {}, {}, 1, &pipeline_create_info,
		nullptr, &handle);
	sbt_wrapper.setup(settings.ctx, settings.ctx->indices.gfx_family.value(), settings.rt_props);
	sbt_wrapper.create(handle, pipeline_create_info);
}

void Pipeline::create_compute_pipeline(const Shader& shader, uint32_t desc_set_layout_cnt,
	VkDescriptorSetLayout* desc_sets,
	std::vector<uint32_t> specialization_data,
	uint32_t push_const_size) {

	std::vector<VkSpecializationMapEntry> entries(specialization_data.size());
	for (int i = 0; i < entries.size(); i++) {
		entries[i].constantID = i;
		entries[i].size = sizeof(uint32_t);
		entries[i].offset = i * sizeof(uint32_t);
	}
	VkSpecializationInfo specialization_info{};
	specialization_info.dataSize = specialization_data.size() * sizeof(uint32_t);
	specialization_info.mapEntryCount = (uint32_t)specialization_data.size();
	specialization_info.pMapEntries = entries.data();
	specialization_info.pData = specialization_data.data();

	VkPushConstantRange push_constant_range{};
	push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_constant_range.offset = 0;
	push_constant_range.size = push_const_size;
	auto compute_shader_module = shader.create_vk_shader_module(device);
	VkPipelineShaderStageCreateInfo shader_stage_create_info = {};
	shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	shader_stage_create_info.module = compute_shader_module;
	if (!specialization_data.empty()) {
		shader_stage_create_info.pSpecializationInfo = &specialization_info;
	}
	shader_stage_create_info.pName = "main";

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
	pipeline_layout_create_info.setLayoutCount = desc_set_layout_cnt;
	pipeline_layout_create_info.pSetLayouts = desc_sets;
	if (push_const_size) {
		pipeline_layout_create_info.pushConstantRangeCount = 1;
		pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;
	}
	pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	vk::check(vkCreatePipelineLayout(device, &pipeline_layout_create_info, NULL, &pipeline_layout));

	VkComputePipelineCreateInfo pipeline_create_info = {};
	pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipeline_create_info.stage = shader_stage_create_info;
	pipeline_create_info.layout = pipeline_layout;
	vk::check(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_create_info, NULL, &handle));
	vkDestroyShaderModule(device, compute_shader_module, nullptr);
}

const std::array<VkStridedDeviceAddressRegionKHR, 4> Pipeline::get_rt_regions() {
	return sbt_wrapper.get_regions();
}

void Pipeline::recompile_pipeline() {
	VkShaderModule vert_shader =
		settings.shaders[0].create_vk_shader_module(device);
	VkShaderModule frag_shader =
		settings.shaders[1].create_vk_shader_module(device);
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
	VkPipelineShaderStageCreateInfo shader_stages[] = { vert_shader_CI,
													   frag_shader_CI };
	pipeline_CI.pStages = shader_stages;

	vertex_input_state_CI.vertexAttributeDescriptionCount =
		static_cast<uint32_t>(settings.attribute_desc.size());
	vertex_input_state_CI.pVertexAttributeDescriptions =
		settings.attribute_desc.data();
	vertex_input_state_CI.vertexBindingDescriptionCount =
		static_cast<uint32_t>(settings.binding_desc.size());
	vertex_input_state_CI.pVertexBindingDescriptions =
		settings.binding_desc.data();
	pipeline_CI.pVertexInputState = &vertex_input_state_CI;
	vk::check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_CI,
		nullptr, &handle),
		"Failed to create pipeline");
	vkDestroyShaderModule(device, frag_shader, nullptr);
	vkDestroyShaderModule(device, vert_shader, nullptr);
}

void Pipeline::cleanup() {
	if (handle != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, handle, nullptr);
	}
	// Note: pipeline layout is usually given exernally, but in the case
	// of compute shaders, it's allocated internally
	if (pipeline_layout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	}

	if (sbt_wrapper.m_ctx) {
		sbt_wrapper.destroy();
	}

	running = false;
	std::unique_lock<std::mutex> tracker_lk(mut);
	cv.wait(tracker_lk, [this] { return tracking_stopped; });
}

