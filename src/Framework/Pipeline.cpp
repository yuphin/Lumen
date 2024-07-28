#include "../LumenPCH.h"
#include "Pipeline.h"
#include "VkUtils.h"

namespace lumen {
	
static uint32_t get_bindings_for_shader_set(const std::vector<Shader>& shaders, VkDescriptorType* descriptor_types) {
	uint32_t binding_mask = 0;
	for (const auto& shader : shaders) {
		for (uint32_t i = 0; i < 32; ++i) {
			if (shader.binding_mask & (1 << i)) {
				if (binding_mask & (1 << i)) {
					LUMEN_ASSERT(descriptor_types[i] == shader.descriptor_types[i],
								 "Binding mask mismatch on shader {}", shader.filename.c_str());
				} else {
					descriptor_types[i] = shader.descriptor_types[i];
					binding_mask |= 1 << i;
				}
			}
		}
	}
	return binding_mask;
}

Pipeline::Pipeline(const std::string& name) : name(name) {}

void Pipeline::reload() {}

void Pipeline::refresh() {
	if (type == PipelineType::RT) {
		sbt_wrapper.destroy();
	}
}

void Pipeline::create_gfx_pipeline(const GraphicsPassSettings& settings, const std::vector<uint32_t>& descriptor_counts,
								   std::vector<Texture2D*> color_outputs, Texture2D* depth_output) {
	LUMEN_ASSERT(color_outputs.size(), "No color outputs for GFX pipeline");
	type = PipelineType::GFX;
	binding_mask = get_bindings_for_shader_set(settings.shaders, descriptor_types);
	create_set_layout(settings.shaders, descriptor_counts);
	for (const auto& shader : settings.shaders) {
		if (push_constant_size && shader.push_constant_size) {
			LUMEN_ASSERT(push_constant_size == shader.push_constant_size,
						 "Currently all shaders only support 1 push constant!");
		}
		if (shader.push_constant_size) {
			push_constant_size = shader.push_constant_size;
		}
	}
	create_pipeline_layout(settings.shaders, {push_constant_size});
	create_update_template(settings.shaders, descriptor_counts);

	VkSpecializationInfo specialization_info = {};
	std::vector<VkSpecializationMapEntry> entries(settings.specialization_data.size());
	for (int i = 0; i < entries.size(); i++) {
		entries[i].constantID = i;
		entries[i].size = sizeof(uint32_t);
		entries[i].offset = i * sizeof(uint32_t);
	}
	specialization_info.mapEntryCount = (uint32_t)entries.size();
	specialization_info.pMapEntries = entries.data();
	specialization_info.dataSize = settings.specialization_data.size() * sizeof(uint32_t);
	specialization_info.pData = settings.specialization_data.data();

	std::vector<VkPipelineShaderStageCreateInfo> stages;
	for (const auto& shader : settings.shaders) {
		VkPipelineShaderStageCreateInfo stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		stage.stage = shader.stage;
		stage.module = shader.create_vk_shader_module(VulkanContext::device);
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

	std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_states;
	if (settings.blend_enables.empty()) {
		blend_attachment_states = std::vector<VkPipelineColorBlendAttachmentState>(
			color_outputs.size(),
			vk::pipeline_color_blend_attachment_state(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
														  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
													  VK_FALSE));
	} else {
		for (bool blend_enable : settings.blend_enables) {
			blend_attachment_states.push_back(
				vk::pipeline_color_blend_attachment_state(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
															  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
														  blend_enable));
		}
	}

	VkPipelineColorBlendStateCreateInfo color_blend =
		vk::pipeline_color_blend_state((uint32_t)blend_attachment_states.size(), blend_attachment_states.data());
	color_blend.logicOpEnable = VK_FALSE;
	color_blend.logicOp = VK_LOGIC_OP_COPY;
	color_blend.blendConstants[0] = 0.0f;
	color_blend.blendConstants[1] = 0.0f;
	color_blend.blendConstants[2] = 0.0f;
	color_blend.blendConstants[3] = 0.0f;
	std::vector<VkDynamicState> dynamic_enables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamic_state_CI =
		vk::pipeline_dynamic_state(dynamic_enables.data(), static_cast<uint32_t>(dynamic_enables.size()));

	std::vector<VkVertexInputBindingDescription> binding_descs;
	std::vector<VkVertexInputAttributeDescription> attribute_descs;
	size_t vert_shader_idx = 0;
	for (int i = 0; i < settings.shaders.size(); i++) {
		if (settings.shaders[i].stage == VK_SHADER_STAGE_VERTEX_BIT) {
			vert_shader_idx = i;
			break;
		}
	}
	auto& vert_shader = settings.shaders[vert_shader_idx];
	int i = 0;
	for (auto& [format, size] : vert_shader.vertex_inputs) {
		auto binding_desc = vk::vertex_input_binding_description(i, size, VK_VERTEX_INPUT_RATE_VERTEX);
		auto attribute_desc = vk::vertex_input_attribute_description(i, i, format, 0);
		binding_descs.push_back(binding_desc);
		attribute_descs.push_back(attribute_desc);
	}
	auto vertex_input_state = vk::pipeline_vertex_input_state();
	vertex_input_state.vertexAttributeDescriptionCount = (uint32_t)attribute_descs.size();
	vertex_input_state.pVertexAttributeDescriptions = attribute_descs.data();
	vertex_input_state.vertexBindingDescriptionCount = (uint32_t)binding_descs.size();
	vertex_input_state.pVertexBindingDescriptions = binding_descs.data();

	VkFormat depth_format = VK_FORMAT_UNDEFINED;
	std::vector<VkFormat> output_formats;
	output_formats.reserve(color_outputs.size());
	for (Texture2D* color_output : color_outputs) {
		output_formats.push_back(color_output->format);
	}
	if (depth_output) {
		depth_format = depth_output->format;
	}
	VkPipelineRenderingCreateInfo pipeline_rendering_create_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = (uint32_t)output_formats.size(),
		.pColorAttachmentFormats = output_formats.data(),
		.depthAttachmentFormat = depth_format};

	auto depth_stencil_state_ci = vk::pipeline_depth_stencil(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	VkGraphicsPipelineCreateInfo pipeline_CI = vk::graphics_pipeline();
	pipeline_CI.pNext = nullptr;
	pipeline_CI.stageCount = (uint32_t)stages.size();
	pipeline_CI.pStages = stages.data();
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

	vk::check(vkCreateGraphicsPipelines(VulkanContext::device, VK_NULL_HANDLE, 1, &pipeline_CI, nullptr, &handle),
			  "Failed to create pipeline");
	for (auto& stage : stages) {
		vkDestroyShaderModule(VulkanContext::device, stage.module, nullptr);
	}
	if (!name.empty()) {
		vk::DebugMarker::set_resource_name(VulkanContext::device, (uint64_t)handle, name.c_str(), VK_OBJECT_TYPE_PIPELINE);
	}
}

void Pipeline::create_rt_pipeline(const RTPassSettings& settings, const std::vector<uint32_t>& descriptor_counts) {
	type = PipelineType::RT;
	binding_mask = get_bindings_for_shader_set(settings.shaders, descriptor_types);
	create_set_layout(settings.shaders, descriptor_counts);
	for (const auto& shader : settings.shaders) {
		if (push_constant_size && shader.push_constant_size) {
			LUMEN_ASSERT(push_constant_size == shader.push_constant_size,
						 "Currently all shaders only support 1 push constant!");
		}
		if (shader.push_constant_size) {
			push_constant_size = shader.push_constant_size;
		}
	}
	create_pipeline_layout(settings.shaders, {push_constant_size});
	create_update_template(settings.shaders, descriptor_counts);

	std::vector<VkSpecializationMapEntry> entries(settings.specialization_data.size());
	for (int i = 0; i < entries.size(); i++) {
		entries[i].constantID = i;
		entries[i].size = sizeof(uint32_t);
		entries[i].offset = i * sizeof(uint32_t);
	}

	std::vector<VkPipelineShaderStageCreateInfo> stages;
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

	VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	stage.pName = "main";

	int stage_idx = 0;
	for (const auto& shader : settings.shaders) {
		VkRayTracingShaderGroupCreateInfoKHR group{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
		group.anyHitShader = VK_SHADER_UNUSED_KHR;
		group.closestHitShader = VK_SHADER_UNUSED_KHR;
		group.generalShader = VK_SHADER_UNUSED_KHR;
		group.intersectionShader = VK_SHADER_UNUSED_KHR;

		stage.module = shader.create_vk_shader_module(VulkanContext::device);
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

	VkSpecializationInfo specialization_info = {};
	if (!settings.specialization_data.empty()) {
		specialization_info.dataSize = settings.specialization_data.size() * sizeof(uint32_t);
		specialization_info.mapEntryCount = (uint32_t)settings.specialization_data.size();
		specialization_info.pMapEntries = entries.data();
		specialization_info.pData = settings.specialization_data.data();
		for (auto& stage : stages) {
			stage.pSpecializationInfo = &specialization_info;
		}
	}
	VkRayTracingPipelineCreateInfoKHR pipeline_CI = {VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
	pipeline_CI.stageCount = static_cast<uint32_t>(stages.size());
	pipeline_CI.pStages = stages.data();
	pipeline_CI.groupCount = static_cast<uint32_t>(groups.size());
	pipeline_CI.pGroups = groups.data();
	pipeline_CI.maxPipelineRayRecursionDepth = settings.recursion_depth;
	pipeline_CI.layout = pipeline_layout;
	pipeline_CI.flags = 0;
	vk::check(vkCreateRayTracingPipelinesKHR(VulkanContext::device, {}, {}, 1, &pipeline_CI, nullptr, &handle), "Failed to create RT pipeline");
	sbt_wrapper.setup(VulkanContext::queue_indices.gfx_family.value(), VulkanContext::rt_props);
	sbt_wrapper.create(handle, pipeline_CI);
	if (!name.empty()) {
		vk::DebugMarker::set_resource_name(VulkanContext::device, (uint64_t)handle, name.c_str(), VK_OBJECT_TYPE_PIPELINE);
	}
	for (auto& stage : stages) {
		vkDestroyShaderModule(VulkanContext::device, stage.module, nullptr);
	}
}

void Pipeline::create_compute_pipeline(const ComputePassSettings& settings,
									   const std::vector<uint32_t>& descriptor_counts) {
	type = PipelineType::COMPUTE;
	binding_mask = get_bindings_for_shader_set({settings.shader}, descriptor_types);
	create_set_layout({settings.shader}, descriptor_counts);
	if (settings.shader.push_constant_size > 0) {
		push_constant_size = settings.shader.push_constant_size;
		create_pipeline_layout({settings.shader}, {push_constant_size});
	} else {
		create_pipeline_layout({settings.shader}, {});
	}
	create_update_template({settings.shader}, descriptor_counts);

	auto compute_shader_module = settings.shader.create_vk_shader_module(VulkanContext::device);
	VkPipelineShaderStageCreateInfo shader_stage_ci = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
	shader_stage_ci.pName = "main";
	shader_stage_ci.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	shader_stage_ci.module = compute_shader_module;
	VkSpecializationInfo specialization_info = {};
	std::vector<VkSpecializationMapEntry> entries;
	if (settings.specialization_data.size()) {
		entries.resize(settings.specialization_data.size());
		for (int i = 0; i < entries.size(); i++) {
			entries[i].constantID = i;
			entries[i].size = sizeof(uint32_t);
			entries[i].offset = i * sizeof(uint32_t);
		}
		specialization_info.dataSize = settings.specialization_data.size() * sizeof(uint32_t);
		specialization_info.mapEntryCount = (uint32_t)settings.specialization_data.size();
		specialization_info.pMapEntries = entries.data();
		specialization_info.pData = settings.specialization_data.data();
		shader_stage_ci.pSpecializationInfo = &specialization_info;
	}
	VkComputePipelineCreateInfo pipeline_CI = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	pipeline_CI.stage = shader_stage_ci;
	pipeline_CI.flags |= VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	pipeline_CI.layout = pipeline_layout;
	vk::check(vkCreateComputePipelines(VulkanContext::device, VK_NULL_HANDLE, 1, &pipeline_CI, nullptr, &handle));
	vkDestroyShaderModule(VulkanContext::device, compute_shader_module, nullptr);
	if (!name.empty()) {
		vk::DebugMarker::set_resource_name(VulkanContext::device, (uint64_t)handle, name.c_str(), VK_OBJECT_TYPE_PIPELINE);
	}
}

const std::array<VkStridedDeviceAddressRegionKHR, 4> Pipeline::get_rt_regions() { return sbt_wrapper.get_regions(); }

void Pipeline::cleanup() {
	if (handle) {
		vkDestroyPipeline(VulkanContext::device, handle, nullptr);
	}
	// Note: pipeline layout is usually given externally, but in the case
	// of compute shaders, it's allocated internally
	if (pipeline_layout) {
		vkDestroyPipelineLayout(VulkanContext::device, pipeline_layout, nullptr);
	}

	if (set_layout) {
		vkDestroyDescriptorSetLayout(VulkanContext::device, set_layout, nullptr);
	}

	if (tlas_layout) {
		vkDestroyDescriptorSetLayout(VulkanContext::device, tlas_layout, nullptr);
	}

	sbt_wrapper.destroy();

	if (tlas_descriptor_pool) {
		vkDestroyDescriptorPool(VulkanContext::device, tlas_descriptor_pool, nullptr);
	}

	if (update_template) {
		vkDestroyDescriptorUpdateTemplate(VulkanContext::device, update_template, nullptr);
	}

	running = false;
	std::unique_lock<std::mutex> tracker_lk(mut);
	cv.wait(tracker_lk, [this] { return tracking_stopped; });
}

void Pipeline::create_set_layout(const std::vector<Shader>& shaders, const std::vector<uint32_t>& descriptor_counts) {
	std::vector<VkDescriptorSetLayoutBinding> set_bindings;

	if (descriptor_counts.size()) {
		int idx = 0;
		for (uint32_t i = 0; i < 32; ++i)
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

	VkDescriptorSetLayoutCreateInfo set_create_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	set_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	set_create_info.bindingCount = uint32_t(set_bindings.size());
	set_create_info.pBindings = set_bindings.data();
	vk::check(vkCreateDescriptorSetLayout(VulkanContext::device, &set_create_info, nullptr, &set_layout));
	// Create the set layout for TLAS
	if (type == PipelineType::RT) {
		VkDescriptorSetLayoutBinding binding = {};
		binding.binding = 0;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		binding.descriptorCount = 1;
		binding.pImmutableSamplers = nullptr;
		binding.stageFlags = 0;
		for (const Shader& shader : shaders) {
			binding.stageFlags |= shader.stage;
		}
		set_create_info.flags = 0;
		set_create_info.bindingCount = 1;
		set_create_info.pBindings = &binding;
		vk::check(vkCreateDescriptorSetLayout(VulkanContext::device, &set_create_info, nullptr, &tlas_layout));
	}
}

void Pipeline::create_pipeline_layout(const std::vector<Shader>& shaders,
									  const std::vector<uint32_t> push_const_sizes) {
	VkPipelineLayoutCreateInfo create_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	VkDescriptorSetLayout set_layouts[] = {set_layout, tlas_layout};
	create_info.setLayoutCount = type == PipelineType::RT ? 2 : 1;
	create_info.pSetLayouts = set_layouts;

	for (const Shader& shader : shaders)
		if (shader.uses_push_constants) pc_stages |= shader.stage;

	std::vector<VkPushConstantRange> pcrs;
	for (uint32_t size : push_const_sizes) {
		VkPushConstantRange pcr = {};
		pcr.size = size;
		pcr.stageFlags = pc_stages;
		pcrs.push_back(pcr);
	}
	if (pcrs.size()) {
		create_info.pushConstantRangeCount = (uint32_t)pcrs.size();
		create_info.pPushConstantRanges = pcrs.data();
	}
	vk::check(vkCreatePipelineLayout(VulkanContext::device, &create_info, nullptr, &pipeline_layout));
}

void Pipeline::create_update_template(const std::vector<Shader>& shaders,
									  const std::vector<uint32_t>& descriptor_counts) {
	if (descriptor_counts.empty()) {
		return;
	}
	for (const auto& shader : shaders) {
		for (uint32_t i = 0; i < 32; ++i) {
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
	auto count_ones = [](uint32_t i) -> uint32_t {
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
				return (size_t)0;
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

	std::vector<VkDescriptorUpdateTemplateEntry> entries;
	LUMEN_ASSERT(count_ones(binding_mask) == descriptor_counts.size(),
				 "Descriptor size mismatch! Check shaders or the supplied descriptors.");
	size_t offset = 0;
	int idx = 0;
	size_t tlas_offset = -1;
	size_t tlas_idx = -1;
	for (uint32_t i = 0; i < 32; ++i) {
		if (binding_mask & (1 << i)) {
			VkDescriptorUpdateTemplateEntry entry = {};
			entry.dstBinding = i;
			entry.dstArrayElement = 0;
			entry.descriptorCount = descriptor_counts[idx];
			entry.descriptorType = descriptor_types[i];
			auto desc_info_size = get_desc_info_size(entry.descriptorType);
			entry.offset = offset;
			entry.stride = desc_info_size;
			entries.push_back(entry);
			if (descriptor_types[i] == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR) {
				tlas_offset = offset;
				tlas_idx = i;
			}
			offset += desc_info_size;
			idx++;
		}
	}

	VkDescriptorUpdateTemplateCreateInfo template_create_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO};

	template_create_info.descriptorUpdateEntryCount = uint32_t(entries.size());
	template_create_info.pDescriptorUpdateEntries = entries.data();

	template_create_info.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR;
	template_create_info.descriptorSetLayout = nullptr;
	template_create_info.pipelineBindPoint = get_bind_point();
	template_create_info.pipelineLayout = pipeline_layout;
	vk::check(vkCreateDescriptorUpdateTemplate(VulkanContext::device, &template_create_info, nullptr, &update_template));
}
}  // namespace lumen
