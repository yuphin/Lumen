#include "Pipeline.h"
#include "VkUtils.h"

namespace vk {
// TODO: make shader filename cstr

static u32 get_bindings_for_shader_set(util::Slice<const Shader> shaders, VkDescriptorType* descriptor_types) {
	u32 binding_mask = 0;
	for (const auto& shader : shaders) {
		for (u32 i = 0; i < 32; ++i) {
			if (shader.binding_mask & (1 << i)) {
				if (binding_mask & (1 << i)) {
					LUMEN_ASSERT(descriptor_types[i] == shader.descriptor_types[i],
								 "Binding mask mismatch on shader %s", shader.filename.data);
				} else {
					descriptor_types[i] = shader.descriptor_types[i];
					binding_mask |= 1 << i;
				}
			}
		}
	}
	return binding_mask;
}

Pipeline::Pipeline(lm::String name) : name(name) {}

void Pipeline::create_gfx_pipeline(const PassSettings& settings, util::Slice<u32> descriptor_counts) {
	LUMEN_ASSERT(settings.color_outputs.size, "No color outputs for GFX pipeline");
	assert(name.is_cstr());
	type = PipelineType::GFX;
	util::Slice<const Shader> shaders_slice = {settings.shaders.data, (u64)settings.shaders.size};
	binding_mask = get_bindings_for_shader_set(shaders_slice, descriptor_types);
	create_set_layout(shaders_slice, descriptor_counts);
	for (const auto& shader : settings.shaders) {
		if (push_constant_size && shader.push_constant_size) {
			LUMEN_ASSERT(push_constant_size == shader.push_constant_size,
						 "Currently all shaders only support 1 push constant!");
		}
		if (shader.push_constant_size) {
			push_constant_size = shader.push_constant_size;
		}
	}
	create_pipeline_layout(shaders_slice, {&push_constant_size, 1});
	create_update_template(shaders_slice, descriptor_counts);

	VkSpecializationInfo specialization_info = {};
	lm::SmallArray<VkSpecializationMapEntry, lm::MAX_SPEC_CONSTANTS> spec_map_entries;
	for (i32 i = 0; i < settings.specialization_data.size; i++) {
		VkSpecializationMapEntry& entry = spec_map_entries.push();
		entry.constantID = i;
		entry.size = sizeof(u32);
		entry.offset = i * sizeof(u32);
	}
	specialization_info.mapEntryCount = (u32)spec_map_entries.size;
	specialization_info.pMapEntries = spec_map_entries.data;
	specialization_info.dataSize = settings.specialization_data.size * sizeof(u32);
	specialization_info.pData = settings.specialization_data.data;

	lm::SmallArray<VkPipelineShaderStageCreateInfo, vk::MAX_SHADERS_PER_PASS> stages;
	for (const auto& shader : settings.shaders) {
		VkPipelineShaderStageCreateInfo stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		stage.stage = shader.stage;
		stage.module = shader.create_vk_shader_module(vk::context().device);
		stage.pName = "main";
		stage.pSpecializationInfo = &specialization_info;
		stages.push_back(stage);
	}

	VkPipelineInputAssemblyStateCreateInfo input_asssembly_CI =
		vk::pipeline_vertex_input_assembly_state(settings.topology, 0, VK_FALSE);
	VkPipelineViewportStateCreateInfo viewport_state = vk::pipeline_viewport_state(1, 1, 0);
	VkPipelineRasterizationStateCreateInfo rasterizer =
		vk::pipeline_rasterization_state(settings.polygon_mode, settings.cull_mode, settings.front_face);
	rasterizer.lineWidth = 1.0f;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.depthBiasEnable = VK_FALSE;
	VkPipelineMultisampleStateCreateInfo multisampling = vk::pipeline_multisample_state(VK_SAMPLE_COUNT_1_BIT);
	multisampling.sampleShadingEnable = VK_FALSE;

	lm::SmallArray<VkPipelineColorBlendAttachmentState, MAX_COLOR_ATTACHMENTS> blend_attachment_states;
	if (settings.blend_enables.empty()) {
		for (u64 i = 0; i < settings.color_outputs.size; ++i) {
			blend_attachment_states.push_back(
				vk::pipeline_color_blend_attachment_state(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
															  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
														  VK_FALSE));
		}
	} else {
		for (bool blend_enable : settings.blend_enables) {
			blend_attachment_states.push_back(
				vk::pipeline_color_blend_attachment_state(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
															  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
														  blend_enable));
		}
	}

	VkPipelineColorBlendStateCreateInfo color_blend =
		vk::pipeline_color_blend_state((u32)blend_attachment_states.size, blend_attachment_states.data);
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.logicOp = VK_LOGIC_OP_COPY;
	color_blend.blendConstants[0] = 0.0f;
	color_blend.blendConstants[1] = 0.0f;
	color_blend.blendConstants[2] = 0.0f;
	color_blend.blendConstants[3] = 0.0f;
	lm::SmallArray<VkDynamicState, MAX_DYNAMIC_STATES> dynamic_enables;
	dynamic_enables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	dynamic_enables.push_back(VK_DYNAMIC_STATE_SCISSOR);
	VkPipelineDynamicStateCreateInfo dynamic_state_CI =
		vk::pipeline_dynamic_state(dynamic_enables.data, static_cast<u32>(dynamic_enables.size));

	lm::SmallArray<VkVertexInputBindingDescription, MAX_VERTEX_BINDINGS> binding_descs;
	lm::SmallArray<VkVertexInputAttributeDescription, MAX_VERTEX_ATTRIBUTES> attribute_descs;
	u64 vert_shader_idx = 0;
	for (u64 i = 0; i < settings.shaders.size; i++) {
		if (settings.shaders[i].stage == VK_SHADER_STAGE_VERTEX_BIT) {
			vert_shader_idx = i;
			break;
		}
	}
	auto& vert_shader = settings.shaders[vert_shader_idx];
	i32 i = 0;
	for (const VertexInput& input : vert_shader.vertex_inputs) {
		auto binding_desc = vk::vertex_input_binding_description(i, input.size, VK_VERTEX_INPUT_RATE_VERTEX);
		auto attribute_desc = vk::vertex_input_attribute_description(i, i, input.format, 0);
		binding_descs.push_back(binding_desc);
		attribute_descs.push_back(attribute_desc);
		++i;
	}
	auto vertex_input_state = vk::pipeline_vertex_input_state();
	vertex_input_state.vertexAttributeDescriptionCount = (u32)attribute_descs.size;
	vertex_input_state.pVertexAttributeDescriptions = attribute_descs.data;
	vertex_input_state.vertexBindingDescriptionCount = (u32)binding_descs.size;
	vertex_input_state.pVertexBindingDescriptions = binding_descs.data;

	VkFormat depth_format = VK_FORMAT_UNDEFINED;
	lm::SmallArray<VkFormat, MAX_COLOR_ATTACHMENTS> output_formats;
	for (vk::Texture* color_output : settings.color_outputs) {
		output_formats.push_back(color_output->format);
	}
	if (settings.depth_output) {
		depth_format = settings.depth_output->format;
	}
	VkPipelineRenderingCreateInfo pipeline_rendering_create_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = (u32)output_formats.size,
		.pColorAttachmentFormats = output_formats.data,
		.depthAttachmentFormat = depth_format};

	auto depth_stencil_state_ci = vk::pipeline_depth_stencil(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	VkGraphicsPipelineCreateInfo pipeline_CI = vk::graphics_pipeline();
	pipeline_CI.pNext = nullptr;
	pipeline_CI.stageCount = (u32)stages.size;
	pipeline_CI.pStages = stages.data;
	pipeline_CI.pVertexInputState = &vertex_input_state;
	pipeline_CI.pInputAssemblyState = &input_asssembly_CI;
	pipeline_CI.pViewportState = &viewport_state;
	pipeline_CI.pRasterizationState = &rasterizer;
	pipeline_CI.pMultisampleState = &multisampling;
	pipeline_CI.pColorBlendState = &color_blend;
	pipeline_CI.pDynamicState = &dynamic_state_CI;
	pipeline_CI.layout = pipeline_layout;
	pipeline_CI.renderPass = VK_NULL_HANDLE;
	pipeline_CI.pNext = &pipeline_rendering_create_info;
	pipeline_CI.subpass = 0;
	pipeline_CI.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_CI.pDepthStencilState = &depth_stencil_state_ci;

	vk::check(vkCreateGraphicsPipelines(vk::context().device, VK_NULL_HANDLE, 1, &pipeline_CI, nullptr, &handle));
	for (auto& stage : stages) {
		vkDestroyShaderModule(vk::context().device, stage.module, nullptr);
	}
	if (!name.empty()) {
		vk::set_resource_name(vk::context().device, (u64)handle, name.data, VK_OBJECT_TYPE_PIPELINE);
	}
}

void Pipeline::create_rt_pipeline(const PassSettings& settings, util::Slice<u32> descriptor_counts,
								  u32 num_as_bindings) {
	assert(name.is_cstr());
	type = PipelineType::RT;
	util::Slice<const Shader> shaders_slice = {settings.shaders.data, (u64)settings.shaders.size};
	binding_mask = get_bindings_for_shader_set(shaders_slice, descriptor_types);
	u32 num_as_bindings_in_shader = 0;
	VkShaderStageFlags binding_stage_flags = 0;
	for (const auto& shader : settings.shaders) {
		if (push_constant_size && shader.push_constant_size) {
			LUMEN_ASSERT(push_constant_size == shader.push_constant_size,
						 "Currently all shaders only support 1 push constant!");
		}
		if (shader.push_constant_size) {
			push_constant_size = shader.push_constant_size;
		}
		num_as_bindings_in_shader = glm::max(num_as_bindings_in_shader, shader.num_as_bindings);
		binding_stage_flags |= shader.stage;
	}
	if (num_as_bindings_in_shader == 0) {
		LUMEN_WARN("No AS bindings found in RT shaders for pipeline %s", name.data);
	}
	LUMEN_ASSERT(num_as_bindings_in_shader <= MAX_AS_BINDING_COUNT, "Max 2 AS bindings are supported");
	create_set_layout(shaders_slice, descriptor_counts);
	create_rt_set_layout(binding_stage_flags, num_as_bindings);
	create_pipeline_layout(shaders_slice, {&push_constant_size, 1});
	create_update_template(shaders_slice, descriptor_counts);

	// Descriptor pool for AS descriptors
	auto pool_size = vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, num_as_bindings);
	auto descriptor_pool_ci = vk::descriptor_pool(1, &pool_size, 1);
	descriptor_pool_ci.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	vk::check(vkCreateDescriptorPool(vk::context().device, &descriptor_pool_ci, nullptr, &tlas_descriptor_pool));
	VkDescriptorSetAllocateInfo set_allocate_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	set_allocate_info.descriptorPool = tlas_descriptor_pool;
	set_allocate_info.descriptorSetCount = 1;
	set_allocate_info.pSetLayouts = &tlas_layout;
	vk::check(vkAllocateDescriptorSets(vk::context().device, &set_allocate_info, &tlas_descriptor_set));

	lm::SmallArray<VkSpecializationMapEntry, lm::MAX_SPEC_CONSTANTS> spec_map_entries;
	for (i32 i = 0; i < settings.specialization_data.size; i++) {
		VkSpecializationMapEntry& entry = spec_map_entries.push();
		entry.constantID = i;
		entry.size = sizeof(u32);
		entry.offset = i * sizeof(u32);
	}

	lm::SmallArray<VkPipelineShaderStageCreateInfo, vk::MAX_SHADERS_PER_PASS> stages;
	lm::SmallArray<VkRayTracingShaderGroupCreateInfoKHR, vk::MAX_SHADERS_PER_PASS> groups;

	VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	stage.pName = "main";

	i32 stage_idx = 0;
	for (const auto& shader : settings.shaders) {
		VkRayTracingShaderGroupCreateInfoKHR group{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
		group.anyHitShader = VK_SHADER_UNUSED_KHR;
		group.closestHitShader = VK_SHADER_UNUSED_KHR;
		group.generalShader = VK_SHADER_UNUSED_KHR;
		group.intersectionShader = VK_SHADER_UNUSED_KHR;

		stage.module = shader.create_vk_shader_module(vk::context().device);
		stage.stage = shader.stage;
		switch (shader.stage) {
			case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
			case VK_SHADER_STAGE_MISS_BIT_KHR: {
				group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
				group.generalShader = stage_idx;
				break;
			}
			case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR: {
				group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
				group.closestHitShader = stage_idx;
				break;
			}
			case VK_SHADER_STAGE_ANY_HIT_BIT_KHR: {
				group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
				group.anyHitShader = stage_idx;
				break;
			}
			case VK_SHADER_STAGE_VERTEX_BIT:
			case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			case VK_SHADER_STAGE_GEOMETRY_BIT:
			case VK_SHADER_STAGE_FRAGMENT_BIT:
			case VK_SHADER_STAGE_COMPUTE_BIT:
			case VK_SHADER_STAGE_ALL_GRAPHICS:
			case VK_SHADER_STAGE_ALL:
			case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
			case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
			case VK_SHADER_STAGE_TASK_BIT_NV:
			case VK_SHADER_STAGE_MESH_BIT_NV:
			case VK_SHADER_STAGE_SUBPASS_SHADING_BIT_HUAWEI:
			case VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI:
				break;
		}
		groups.push_back(group);
		stages.push_back(stage);
		stage_idx++;
	}
	LUMEN_ASSERT(groups.size == stages.size, "Currently 1 stage = 1 group");

	VkSpecializationInfo specialization_info = {};
	if (!settings.specialization_data.empty()) {
		specialization_info.dataSize = settings.specialization_data.size * sizeof(u32);
		specialization_info.mapEntryCount = (u32)settings.specialization_data.size;
		specialization_info.pMapEntries = spec_map_entries.data;
		specialization_info.pData = settings.specialization_data.data;
		for (auto& stage : stages) {
			stage.pSpecializationInfo = &specialization_info;
		}
	}
	VkRayTracingPipelineCreateInfoKHR pipeline_CI = {VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
	pipeline_CI.stageCount = static_cast<u32>(stages.size);
	pipeline_CI.pStages = stages.data;
	pipeline_CI.groupCount = static_cast<u32>(groups.size);
	pipeline_CI.pGroups = groups.data;
	pipeline_CI.maxPipelineRayRecursionDepth = settings.recursion_depth;
	pipeline_CI.layout = pipeline_layout;
	pipeline_CI.flags = 0;
	vk::check(vkCreateRayTracingPipelinesKHR(vk::context().device, {}, {}, 1, &pipeline_CI, nullptr, &handle));
	sbt_wrapper.create(handle, pipeline_CI);
	if (!name.empty()) {
		vk::set_resource_name(vk::context().device, (u64)handle, name.data, VK_OBJECT_TYPE_PIPELINE);
	}
	for (auto& shader_stage : stages) {
		vkDestroyShaderModule(vk::context().device, shader_stage.module, nullptr);
	}
}

void Pipeline::create_compute_pipeline(const PassSettings& settings, util::Slice<u32> descriptor_counts) {
	assert(name.is_cstr());
	type = PipelineType::COMPUTE;
	const Shader& shader = settings.shaders[0];
	util::Slice<const Shader> shader_slice(&shader, 1);
	binding_mask = get_bindings_for_shader_set(shader_slice, descriptor_types);

	create_set_layout(shader_slice, descriptor_counts);
	if (shader.push_constant_size > 0) {
		push_constant_size = shader.push_constant_size;
		create_pipeline_layout(shader_slice, {&push_constant_size, 1});
	} else {
		create_pipeline_layout(shader_slice, {});
	}
	create_update_template(shader_slice, descriptor_counts);

	auto compute_shader_module = shader.create_vk_shader_module(vk::context().device);
	VkPipelineShaderStageCreateInfo shader_stage_ci = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	shader_stage_ci.pName = "main";
	shader_stage_ci.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	shader_stage_ci.module = compute_shader_module;
	VkSpecializationInfo specialization_info = {};
	lm::SmallArray<VkSpecializationMapEntry, lm::MAX_SPEC_CONSTANTS> spec_map_entries;
	if (settings.specialization_data.size) {
		for (i32 i = 0; i < settings.specialization_data.size; i++) {
			VkSpecializationMapEntry& entry = spec_map_entries.push();
			entry.constantID = i;
			entry.size = sizeof(u32);
			entry.offset = i * sizeof(u32);
		}
		specialization_info.dataSize = settings.specialization_data.size * sizeof(u32);
		specialization_info.mapEntryCount = (u32)settings.specialization_data.size;
		specialization_info.pMapEntries = spec_map_entries.data;
		specialization_info.pData = settings.specialization_data.data;
		shader_stage_ci.pSpecializationInfo = &specialization_info;
	}
	VkComputePipelineCreateInfo pipeline_CI = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	pipeline_CI.stage = shader_stage_ci;
	pipeline_CI.flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	pipeline_CI.layout = pipeline_layout;
	vk::check(vkCreateComputePipelines(vk::context().device, VK_NULL_HANDLE, 1, &pipeline_CI, nullptr, &handle));
	vkDestroyShaderModule(vk::context().device, compute_shader_module, nullptr);
	if (!name.empty()) {
		vk::set_resource_name(vk::context().device, (u64)handle, name.data, VK_OBJECT_TYPE_PIPELINE);
	}
}

lm::SmallArray<VkStridedDeviceAddressRegionKHR, NUM_SBT_GROUPS> Pipeline::get_rt_regions() { return sbt_wrapper.get_regions(); }

void Pipeline::cleanup() {
	if (handle) {
		vkDestroyPipeline(vk::context().device, handle, nullptr);
	}
	if (pipeline_layout) {
		vkDestroyPipelineLayout(vk::context().device, pipeline_layout, nullptr);
	}

	if (set_layout) {
		vkDestroyDescriptorSetLayout(vk::context().device, set_layout, nullptr);
	}

	if (tlas_layout) {
		vkDestroyDescriptorSetLayout(vk::context().device, tlas_layout, nullptr);
	}

	sbt_wrapper.destroy();

	if (tlas_descriptor_pool) {
		vkDestroyDescriptorPool(vk::context().device, tlas_descriptor_pool, nullptr);
	}

	if (update_template) {
		vkDestroyDescriptorUpdateTemplate(vk::context().device, update_template, nullptr);
	}
}

void Pipeline::create_rt_set_layout(VkShaderStageFlags binding_stage_flags, u32 num_as_bindings) {
	lm::SmallArray<VkDescriptorSetLayoutBinding, MAX_BINDINGS> set_bindings;
	for (u32 i = 0; i < num_as_bindings; ++i) {
		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = i;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = binding_stage_flags;
		set_bindings.push_back(binding);
	}

	VkDescriptorSetLayoutCreateInfo set_create_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	set_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	set_create_info.bindingCount = u32(set_bindings.size);
	set_create_info.pBindings = set_bindings.data;

	VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_ci = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
	lm::SmallArray<VkDescriptorBindingFlags, MAX_BINDINGS> binding_flags;
	binding_flags.resize(num_as_bindings);
	for (u32 i = 0; i < num_as_bindings; ++i) {
		binding_flags[i] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
		// binding_flags[i] = 0;
	}
	binding_flags_ci.bindingCount = num_as_bindings;
	binding_flags_ci.pBindingFlags = binding_flags.data;
	binding_flags_ci.pNext = nullptr;

	set_create_info.pNext = &binding_flags_ci;

	vk::check(vkCreateDescriptorSetLayout(vk::context().device, &set_create_info, nullptr, &tlas_layout));
}

void Pipeline::create_set_layout(util::Slice<const Shader> shaders, util::Slice<u32> descriptor_counts) {
	lm::SmallArray<VkDescriptorSetLayoutBinding, MAX_BINDINGS> set_bindings;

	if (descriptor_counts.size) {
		i32 idx = 0;
		for (u32 i = 0; i < 32; ++i) {
			if (binding_mask & (1 << i)) {
				VkDescriptorSetLayoutBinding binding = {};
				binding.binding = i;
				binding.descriptorType = descriptor_types[i];
				binding.descriptorCount = descriptor_counts[idx];
				binding.pImmutableSamplers = nullptr;

				binding.stageFlags = 0;
				for (const Shader& shader : shaders) {
					if (shader.binding_mask & (1 << i)) {
						binding.stageFlags |= shader.stage;
					}
				}
				set_bindings.push_back(binding);
				idx++;
			}
		}
	}

	VkDescriptorSetLayoutCreateInfo set_create_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	set_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	set_create_info.bindingCount = u32(set_bindings.size);
	set_create_info.pBindings = set_bindings.data;
	vk::check(vkCreateDescriptorSetLayout(vk::context().device, &set_create_info, nullptr, &set_layout));
}

void Pipeline::create_pipeline_layout(util::Slice<const Shader> shaders, util::Slice<u32> push_const_sizes) {
	VkPipelineLayoutCreateInfo create_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	VkDescriptorSetLayout set_layouts[] = {set_layout, tlas_layout};
	create_info.setLayoutCount = type == PipelineType::RT ? 2 : 1;
	create_info.pSetLayouts = set_layouts;

	for (const Shader& shader : shaders)
		if (shader.uses_push_constants) pc_stages |= shader.stage;

	lm::SmallArray<VkPushConstantRange, MAX_PUSH_CONSTANT_RANGES> pcrs;
	for (u32 size : push_const_sizes) {
		VkPushConstantRange pcr = {};
		pcr.size = size;
		pcr.stageFlags = pc_stages;
		pcrs.push_back(pcr);
	}
	if (pcrs.size) {
		create_info.pushConstantRangeCount = (u32)pcrs.size;
		create_info.pPushConstantRanges = pcrs.data;
	}
	vk::check(vkCreatePipelineLayout(vk::context().device, &create_info, nullptr, &pipeline_layout));
}

void Pipeline::create_update_template(util::Slice<const Shader> shaders, util::Slice<u32> descriptor_counts) {
	if (descriptor_counts.empty()) {
		return;
	}
	for (const auto& shader : shaders) {
		for (u32 i = 0; i < 32; ++i) {
			if (shader.binding_mask & (1 << i)) {
				if (binding_mask & (1 << i)) {
					LUMEN_ASSERT(descriptor_types[i] == shader.descriptor_types[i], "Binding mask mismatch");
				} else {
					descriptor_types[i] = shader.descriptor_types[i];
					binding_mask |= 1 << i;
				}
			}
		}
	}

	// https://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
	auto count_ones = [](u32 i) -> u32 {
		i = i - ((i >> 1) & 0x55555555);				 // add pairs of bits
		i = (i & 0x33333333) + ((i >> 2) & 0x33333333);	 // quads
		i = (i + (i >> 4)) & 0x0F0F0F0F;				 // groups of 8
		return (i * 0x01010101) >> 24;
	};

	auto get_desc_info_size = [](VkDescriptorType type) {
		switch (type) {
			case VK_DESCRIPTOR_TYPE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
				return sizeof(VkDescriptorImageInfo);
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				return sizeof(VkDescriptorBufferInfo);
			case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
				return sizeof(VkWriteDescriptorSetAccelerationStructureKHR);

			default:
				LUMEN_ERROR("Unimplemented descriptor type!");
				return (u64)0;
		}
	};

	auto get_bind_point = [this]() {
		switch (type) {
			case PipelineType::GFX: {
				return VK_PIPELINE_BIND_POINT_GRAPHICS;
			}
			case PipelineType::RT: {
				return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
			}
			case PipelineType::COMPUTE: {
				return VK_PIPELINE_BIND_POINT_COMPUTE;
			}
		}
		return VK_PIPELINE_BIND_POINT_MAX_ENUM;
	};

	lm::SmallArray<VkDescriptorUpdateTemplateEntry, MAX_BINDINGS> entries;
	LUMEN_ASSERT(count_ones(binding_mask) == descriptor_counts.size,
				 "Descriptor size mismatch! Check shaders or the supplied descriptors.");
	u64 offset = 0;
	i32 idx = 0;
	for (u32 i = 0; i < MAX_BINDINGS; ++i) {
		if ((binding_mask & (1 << i)) == 0) {
			continue;
		}
		VkDescriptorUpdateTemplateEntry entry = {};
		entry.dstBinding = i;
		entry.dstArrayElement = 0;
		entry.descriptorCount = descriptor_counts[idx];
		entry.descriptorType = descriptor_types[i];
		auto desc_info_size = get_desc_info_size(entry.descriptorType);
		entry.offset = offset;
		entry.stride = desc_info_size;
		entries.push_back(entry);
		offset += desc_info_size;
		idx++;
	}

	VkDescriptorUpdateTemplateCreateInfo template_create_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO};

	template_create_info.descriptorUpdateEntryCount = u32(entries.size);
	template_create_info.pDescriptorUpdateEntries = entries.data;

	template_create_info.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
	template_create_info.descriptorSetLayout = nullptr;
	template_create_info.pipelineBindPoint = get_bind_point();
	template_create_info.pipelineLayout = pipeline_layout;
	vk::check(vkCreateDescriptorUpdateTemplate(vk::context().device, &template_create_info, nullptr, &update_template));
}
}  // namespace vk
