#include "LumenPCH.h"

#include "Pipeline.h"

Pipeline::Pipeline(const VkDevice& device) : device(device) {}

void Pipeline::track_for_changes() {
	for (auto& file : gfx_settings.shaders) {
		const std::filesystem::path path = file.filename;
		paths[file.filename] = std::filesystem::last_write_time(path);
	}
	ThreadPool::submit([this] {
		tracking_stopped = false;
		std::chrono::duration<int, std::milli> delay =
			std::chrono::milliseconds(100);
		while (running) {
			std::this_thread::sleep_for(delay);
			for (auto& file : gfx_settings.shaders) {
				const std::filesystem::path path = file.filename;
				auto last_write = std::filesystem::last_write_time(path);
				if (paths.find(file.filename) != paths.end() &&
					last_write != paths[file.filename]) {
					LUMEN_TRACE("Shader changed: {0}", file.filename);
					paths[file.filename] = last_write;
					if (!file.compile()) {
						gfx_cis.pipeline_CI.flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
						gfx_cis.pipeline_CI.basePipelineHandle = handle;
						gfx_cis.pipeline_CI.basePipelineIndex = -1;
						VkPipeline old_handle = handle;
						this->recompile_pipeline();
						EventHandler::obsolete_pipelines.push_back({ old_handle, this });
						EventHandler::set(LumenEvent::SHADER_RELOAD);
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

void Pipeline::refresh() {
	if (type == PipelineType::RT) {
		sbt_wrapper.destroy();
		sbt_wrapper = sbt_wrapper_tmp;
	}
}

void Pipeline::reload() {
	LUMEN_TRACE("Reloading pipeline");
	std::array<size_t, 3> shader_sizes = {
		gfx_settings.shaders.size(),
		rt_settings.shaders.size(),
		1
	};
	std::array<Shader*, 3> shaders = {
		gfx_settings.shaders.data(),
		rt_settings.shaders.data(),
		&compute_settings.shader
	};
	size_t sz = shader_sizes[(int)type];
	Shader* shader_data = shaders[(int)type];
	for (size_t i = 0; i < sz; i++) {
		auto& file = shader_data[i];
		if (!file.compile()) {
			if (type == PipelineType::GFX) {
				gfx_cis.pipeline_CI.flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
				gfx_cis.pipeline_CI.basePipelineHandle = handle;
				gfx_cis.pipeline_CI.basePipelineIndex = -1;
			} else if (type == PipelineType::RT) {
				rt_cis.pipeline_CI.flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
				rt_cis.pipeline_CI.basePipelineHandle = handle;
				rt_cis.pipeline_CI.basePipelineIndex = -1;
			} else {
				compute_cis.pipeline_CI.flags |= VK_PIPELINE_CREATE_DERIVATIVE_BIT;
				compute_cis.pipeline_CI.basePipelineHandle = handle;
				compute_cis.pipeline_CI.basePipelineIndex = -1;
			}

			EventHandler::obsolete_pipelines.push_back({ handle, this });
			this->recompile_pipeline(type);
			EventHandler::set(LumenEvent::SHADER_RELOAD);
		}
	}
}

void Pipeline::create_gfx_pipeline(const GraphicsPipelineSettings& settings) {
	LUMEN_ASSERT(settings.pipeline_layout, "Pipeline layout cannot be null");
	LUMEN_ASSERT(settings.render_pass, "Render pass cannot be null");
	type = PipelineType::GFX;
	gfx_cis.input_asssembly_CI = vk::pipeline_vertex_input_assembly_state_CI(
		settings.topology, 0, VK_FALSE);
	gfx_cis.viewport_state = vk::pipeline_viewport_state_CI(1, 1, 0);
	gfx_cis.rasterizer = vk::pipeline_rasterization_state_CI(
		settings.polygon_mode, settings.cull_mode, settings.front_face);
	gfx_cis.rasterizer.lineWidth = settings.line_width;
	gfx_cis.rasterizer.depthClampEnable = VK_FALSE;
	gfx_cis.rasterizer.rasterizerDiscardEnable = VK_FALSE;
	gfx_cis.rasterizer.depthBiasEnable = VK_FALSE;
	gfx_cis.multisampling = vk::pipeline_multisample_state_CI(settings.sample_count);
	gfx_cis.multisampling.sampleShadingEnable = VK_FALSE;
	gfx_cis.color_blend_attachment = vk::pipeline_color_blend_attachment_state(
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		settings.blend_enable);
	gfx_cis.color_blend = vk::pipeline_color_blend_state_CI(1, &gfx_cis.color_blend_attachment);
	gfx_cis.color_blend.logicOpEnable = VK_FALSE;
	gfx_cis.color_blend.logicOp = VK_LOGIC_OP_COPY;
	gfx_cis.color_blend.blendConstants[0] = 0.0f;
	gfx_cis.color_blend.blendConstants[1] = 0.0f;
	gfx_cis.color_blend.blendConstants[2] = 0.0f;
	gfx_cis.color_blend.blendConstants[3] = 0.0f;
	gfx_cis.dynamic_state_CI = vk::pipeline_dynamic_state_CI(
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

	gfx_cis.vert_shader_CI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	gfx_cis.vert_shader_CI.stage = VK_SHADER_STAGE_VERTEX_BIT;
	gfx_cis.vert_shader_CI.module = vert_shader;
	gfx_cis.vert_shader_CI.pName = "main";
	gfx_cis.vert_shader_CI.pNext = nullptr;
	gfx_cis.vert_shader_CI.flags = 0;
	gfx_cis.vert_shader_CI.pSpecializationInfo = nullptr;

	gfx_cis.frag_shader_CI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	gfx_cis.frag_shader_CI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	gfx_cis.frag_shader_CI.module = frag_shader;
	gfx_cis.frag_shader_CI.pName = "main";
	gfx_cis.frag_shader_CI.pNext = nullptr;
	gfx_cis.frag_shader_CI.flags = 0;
	gfx_cis.frag_shader_CI.pSpecializationInfo = nullptr;
	VkPipelineShaderStageCreateInfo shader_stages[] = { gfx_cis.vert_shader_CI,
													   gfx_cis.frag_shader_CI };

	auto depth_stencil_state_ci =
		vk::pipeline_depth_stencil_CI(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	gfx_cis.pipeline_CI = vk::graphics_pipeline_CI();
	gfx_cis.pipeline_CI.pNext = nullptr;
	gfx_cis.pipeline_CI.stageCount = 2;
	gfx_cis.pipeline_CI.pStages = shader_stages;
	gfx_cis.pipeline_CI.pVertexInputState = &vertex_input_state;
	gfx_cis.pipeline_CI.pInputAssemblyState = &gfx_cis.input_asssembly_CI;
	gfx_cis.pipeline_CI.pViewportState = &gfx_cis.viewport_state;
	gfx_cis.pipeline_CI.pRasterizationState = &gfx_cis.rasterizer;
	gfx_cis.pipeline_CI.pMultisampleState = &gfx_cis.multisampling;
	gfx_cis.pipeline_CI.pColorBlendState = &gfx_cis.color_blend;
	gfx_cis.pipeline_CI.pDynamicState = &gfx_cis.dynamic_state_CI;
	gfx_cis.pipeline_CI.layout = settings.pipeline_layout;
	gfx_cis.pipeline_CI.renderPass = settings.render_pass;
	gfx_cis.pipeline_CI.subpass = 0;
	gfx_cis.pipeline_CI.basePipelineHandle = VK_NULL_HANDLE;
	gfx_cis.pipeline_CI.pDepthStencilState = &depth_stencil_state_ci;

	this->handle = settings.pipeline;
	this->gfx_settings = settings;

	if (settings.enable_tracking) {
		gfx_cis.pipeline_CI.flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	}
	vk::check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gfx_cis.pipeline_CI,
			  nullptr, &handle),
			  "Failed to create pipeline");

	vkDestroyShaderModule(device, frag_shader, nullptr);
	vkDestroyShaderModule(device, vert_shader, nullptr);
}

void Pipeline::create_rt_pipeline(RTPipelineSettings& settings,
								  const std::vector<uint32_t> specialization_data) {
	VkPipelineLayoutCreateInfo pipeline_layout_create_info{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipeline_layout_create_info.setLayoutCount = (uint32_t)settings.desc_layouts.size();
	pipeline_layout_create_info.pSetLayouts = settings.desc_layouts.data();

	type = PipelineType::RT;
	auto pcr_cnt = (uint32_t)settings.push_consts.size();
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

	rt_settings = settings;
	rt_cis.stages = rt_settings.stages;
	rt_cis.groups = rt_settings.groups;
	rt_cis.max_recursion_depth = rt_settings.max_recursion_depth;
	rt_cis.pipeline_CI.stageCount =
		static_cast<uint32_t>(rt_cis.stages.size());
	rt_cis.pipeline_CI.pStages = rt_cis.stages.data();
	rt_cis.pipeline_CI.groupCount = static_cast<uint32_t>(rt_cis.groups.size());
	rt_cis.pipeline_CI.pGroups = rt_cis.groups.data();
	rt_cis.pipeline_CI.maxPipelineRayRecursionDepth = rt_cis.max_recursion_depth;
	rt_cis.pipeline_CI.layout = pipeline_layout;
	rt_cis.pipeline_CI.flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;

	vkCreateRayTracingPipelinesKHR(device, {}, {}, 1, &rt_cis.pipeline_CI,
								   nullptr, &handle);
	sbt_wrapper.setup(rt_settings.ctx, rt_settings.ctx->indices.gfx_family.value(), rt_settings.rt_props);
	sbt_wrapper.create(handle, rt_cis.pipeline_CI);
}

void Pipeline::create_compute_pipeline(const ComputePipelineSettings& settings) {
	type = PipelineType::COMPUTE;
	compute_settings = settings;
	std::vector<VkSpecializationMapEntry> entries(compute_settings.specialization_data.size());
	for (int i = 0; i < entries.size(); i++) {
		entries[i].constantID = i;
		entries[i].size = sizeof(uint32_t);
		entries[i].offset = i * sizeof(uint32_t);
	}
	compute_cis.specialization_info.dataSize = 
		compute_settings.specialization_data.size() * sizeof(uint32_t);
	compute_cis.specialization_info.mapEntryCount = 
		(uint32_t)compute_settings.specialization_data.size();
	compute_cis.specialization_info.pMapEntries = entries.data();
	compute_cis.specialization_info.pData = compute_settings.specialization_data.data();

	VkPushConstantRange push_constant_range{};
	push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_constant_range.offset = 0;
	push_constant_range.size = compute_settings.push_const_size;
	auto compute_shader_module = compute_settings.shader.create_vk_shader_module(device);
	compute_cis.shader_stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	compute_cis.shader_stage_ci.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	compute_cis.shader_stage_ci.module = compute_shader_module;
	if (!compute_settings.specialization_data.empty()) {
		compute_cis.shader_stage_ci.pSpecializationInfo = &compute_cis.specialization_info;
	}
	compute_cis.shader_stage_ci.pName = "main";

	VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
	pipeline_layout_create_info.setLayoutCount = compute_settings.desc_set_layout_cnt;
	pipeline_layout_create_info.pSetLayouts = compute_settings.desc_set_layouts;
	if (compute_settings.push_const_size) {
		pipeline_layout_create_info.pushConstantRangeCount = 1;
		pipeline_layout_create_info.pPushConstantRanges = &push_constant_range;
	}
	pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	vk::check(vkCreatePipelineLayout(device, &pipeline_layout_create_info, NULL, &pipeline_layout));
	compute_cis.pipeline_CI.stage = compute_cis.shader_stage_ci;
	compute_cis.pipeline_CI.flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	compute_cis.pipeline_CI.layout = pipeline_layout;
	vk::check(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_cis.pipeline_CI, NULL, &handle));
	vkDestroyShaderModule(device, compute_shader_module, nullptr);
}

const std::array<VkStridedDeviceAddressRegionKHR, 4> Pipeline::get_rt_regions() {
	return sbt_wrapper.get_regions();
}

void Pipeline::recompile_pipeline(PipelineType type) {
	switch (type) {
		case PipelineType::GFX:
		{
			VkShaderModule vert_shader =
				gfx_settings.shaders[0].create_vk_shader_module(device);
			VkShaderModule frag_shader =
				gfx_settings.shaders[1].create_vk_shader_module(device);
			auto vertex_input_state_CI = vk::pipeline_vertex_input_state_CI();
			gfx_cis.vert_shader_CI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			gfx_cis.vert_shader_CI.stage = VK_SHADER_STAGE_VERTEX_BIT;
			gfx_cis.vert_shader_CI.module = vert_shader;
			gfx_cis.vert_shader_CI.pName = "main";
			gfx_cis.vert_shader_CI.pNext = nullptr;
			gfx_cis.vert_shader_CI.flags = 0;
			gfx_cis.vert_shader_CI.pSpecializationInfo = nullptr;

			gfx_cis.frag_shader_CI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			gfx_cis.frag_shader_CI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			gfx_cis.frag_shader_CI.module = frag_shader;
			gfx_cis.frag_shader_CI.pName = "main";
			gfx_cis.frag_shader_CI.pNext = nullptr;
			gfx_cis.frag_shader_CI.flags = 0;
			gfx_cis.frag_shader_CI.pSpecializationInfo = nullptr;
			VkPipelineShaderStageCreateInfo shader_stages[] = { gfx_cis.vert_shader_CI,
															   gfx_cis.frag_shader_CI };
			gfx_cis.pipeline_CI.pStages = shader_stages;

			vertex_input_state_CI.vertexAttributeDescriptionCount =
				static_cast<uint32_t>(gfx_settings.attribute_desc.size());
			vertex_input_state_CI.pVertexAttributeDescriptions =
				gfx_settings.attribute_desc.data();
			vertex_input_state_CI.vertexBindingDescriptionCount =
				static_cast<uint32_t>(gfx_settings.binding_desc.size());
			vertex_input_state_CI.pVertexBindingDescriptions =
				gfx_settings.binding_desc.data();
			gfx_cis.pipeline_CI.pVertexInputState = &vertex_input_state_CI;
			vk::check(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gfx_cis.pipeline_CI,
					  nullptr, &handle),
					  "Failed to create pipeline");
			vkDestroyShaderModule(device, frag_shader, nullptr);
			vkDestroyShaderModule(device, vert_shader, nullptr);

		}
		break;
		case PipelineType::RT:
		{
			for (int i = 0; i < rt_settings.stages.size(); i++) {
				rt_settings.stages[i].module = rt_settings.shaders[i].create_vk_shader_module(device);
			}
			rt_cis.stages = rt_settings.stages;
			vkCreateRayTracingPipelinesKHR(device, {}, {}, 1, &rt_cis.pipeline_CI,
										   nullptr, &handle);
			sbt_wrapper_tmp.setup(rt_settings.ctx, rt_settings.ctx->indices.gfx_family.value(), rt_settings.rt_props);
			sbt_wrapper_tmp.create(handle, rt_cis.pipeline_CI);
			for (int i = 0; i < rt_settings.stages.size(); i++) {
				vkDestroyShaderModule(device, rt_settings.stages[i].module, nullptr);
			}
		}
		break;
		case PipelineType::COMPUTE:
		{
			compute_cis.shader_stage_ci.module = compute_settings.shader.create_vk_shader_module(device);
			compute_cis.pipeline_CI.stage = compute_cis.shader_stage_ci;
			vk::check(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compute_cis.pipeline_CI, NULL, &handle));
			vkDestroyShaderModule(device, compute_cis.shader_stage_ci.module, nullptr);

		}
		break;
		default:
			break;
	}
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

