#include "LumenPCH.h"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "RTScene.h"
const bool USE_RTX = true;
//TODO: Use instances in the rasterization pipeline
//TODO: Use a single scratch buffer

RTScene* RTScene::instance = nullptr;

static void fb_resize_callback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<RTScene*>(glfwGetWindowUserPointer(window));
	app->resized = true;
}

RTScene::RTScene(int width, int height, bool debug) :
	Scene(width, height, debug) {
	this->instance = this;
}

void RTScene::init(Window* window) {
	this->window = window;
	vkb.ctx.window_ptr = window->get_window_ptr();
	glfwSetFramebufferSizeCallback(vkb.ctx.window_ptr, fb_resize_callback);
	// Init with ray tracing extensions
	vkb.add_device_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	vkb.add_device_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
	vkb.add_device_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	vkb.create_instance();
	if (vkb.enable_validation_layers) {
		vkb.setup_debug_messenger();
	}
	vkb.create_surface();
	vkb.pick_physical_device();
	vkb.create_logical_device();
	vkb.create_swapchain();
	create_default_render_pass(vkb.ctx);
	vkb.create_framebuffers(vkb.ctx.default_render_pass);
	vkb.create_command_pool();
	vkb.create_command_buffers();
	vkb.create_sync_primitives();
	initialized = true;

	// Requesting ray tracing properties
	VkPhysicalDeviceProperties2 prop2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
	prop2.pNext = &rt_props;
	vkGetPhysicalDeviceProperties2(vkb.ctx.physical_device, &prop2);

	init_scene();
	auto cam_ptr = camera.get();
	window->add_mouse_move_callback([window, cam_ptr](double delta_x, double delta_y) {
		if (window->is_mouse_held(MouseAction::LEFT)) {
			cam_ptr->rotate(0.05f * (float)delta_y, -0.05f * (float)delta_x, 0.0f);
		}
	});
	create_offscreen_resources();
	create_descriptors();
	create_graphics_pipeline();
	create_uniform_buffers();
	update_descriptors();
	if (USE_RTX) {
		create_blas();
		create_tlas();
		create_rt_descriptors();
		create_rt_pipeline();
		create_rt_sbt();
	}
	create_post_descriptor();
	update_post_desc_set();
	create_post_pipeline();

	//init_imgui();
}

void RTScene::init_scene() {
	constexpr int VERTEX_BINDING_ID = 0;
	camera = std::unique_ptr<PerspectiveCamera>(
		new PerspectiveCamera(45.0f, 0.1f, 1000.0f, (float)width / height,
		glm::vec3(1.25, 1.5, 6.5))
		);
	std::string filename = "scenes/cornellBox.gltf";
	using vkBU = VkBufferUsageFlagBits;
	tinygltf::Model tmodel;
	tinygltf::TinyGLTF tcontext;
	std::string warn, error;
	LUMEN_TRACE("Loading file: {}", filename.c_str());
	if (!tcontext.LoadASCIIFromFile(&tmodel, &error, &warn, filename)) {
		assert(!"Error while loading scene");
	}
	if (!warn.empty()) {
		LUMEN_WARN(warn.c_str());
	}
	if (!error.empty()) {
		LUMEN_ERROR(error.c_str());
	}
	gltf_scene.import_materials(tmodel);
	gltf_scene.import_drawable_nodes(tmodel, GltfAttributes::Normal | GltfAttributes::Texcoord_0);

	auto vertex_buf_size = gltf_scene.positions.size() * sizeof(glm::vec3);
	auto idx_buf_size = gltf_scene.indices.size() * sizeof(uint32_t);
	std::vector<MaterialPushConst> materials;
	std::vector<PrimMeshInfo> prim_lookup;
	for (const auto& m : gltf_scene.materials) {
		materials.push_back({ m.base_color_factor });
	}

	for (auto& pm : gltf_scene.prim_meshes) {
		prim_lookup.push_back({ pm.first_idx, pm.vtx_offset, pm.material_idx });
	}
	vertex_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		vertex_buf_size,
		gltf_scene.positions.data(),
		true
	);
	index_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		idx_buf_size,
		gltf_scene.indices.data(),
		true
	);

	normal_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		gltf_scene.normals.size() * sizeof(gltf_scene.normals[0]),
		gltf_scene.normals.data(),
		true
	);
	uv_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		gltf_scene.texcoords0.size() * sizeof(gltf_scene.texcoords0[0]),
		gltf_scene.texcoords0.data(),
		true
	);
	materials_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		materials.size() * sizeof(GLTFMaterial),
		materials.data(),
		true
	);
	prim_lookup_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		prim_lookup.size() * sizeof(PrimMeshInfo),
		prim_lookup.data(),
		true
	);
	SceneDesc desc;
	desc.vertex_addr = vertex_buffer.get_device_address();
	desc.index_addr = index_buffer.get_device_address();
	desc.normal_addr = normal_buffer.get_device_address();
	desc.uv_addr = uv_buffer.get_device_address();
	desc.material_addr = materials_buffer.get_device_address();
	desc.prim_info_addr = prim_lookup_buffer.get_device_address();

	scene_desc_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		sizeof(SceneDesc),
		&desc,
		true
	);
	instances.emplace_back(ModelInstance{ glm::mat4(1.0f), (uint32_t)instances.size() });
}

void RTScene::create_blas() {
	std::vector<BlasInput> blas_inputs;
	auto vertex_address = get_device_address(vkb.ctx.device, vertex_buffer.handle);
	auto idx_address = get_device_address(vkb.ctx.device, index_buffer.handle);
	for (auto& primMesh : gltf_scene.prim_meshes) {
		BlasInput geo = to_vk_geometry(primMesh, vertex_address, idx_address);
		blas_inputs.push_back({ geo });
	}
	vkb.build_blas(blas_inputs, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void RTScene::create_tlas() {
	std::vector<VkAccelerationStructureInstanceKHR> tlas;
	tlas.reserve(instances.size());
	for (const ModelInstance& inst : instances) {
		VkAccelerationStructureInstanceKHR rayInst{};
		rayInst.transform = to_vk_matrix(inst.transform);
		rayInst.instanceCustomIndex = inst.idx;
		rayInst.accelerationStructureReference = vkb.get_blas_device_address(inst.idx);
		rayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		rayInst.mask = 0xFF; //  Only be hit if rayMask & instance.mask != 0
		rayInst.instanceShaderBindingTableRecordOffset = 0;	// We will use the same hit group for all objects
		tlas.emplace_back(rayInst);
	}

	for (auto& node : gltf_scene.nodes) {
		VkAccelerationStructureInstanceKHR rayInst{};
		rayInst.transform = to_vk_matrix(node.world_matrix);
		rayInst.instanceCustomIndex = node.prim_mesh;  // gl_InstanceCustomIndexEXT: to find which primitive
		rayInst.accelerationStructureReference = vkb.get_blas_device_address(node.prim_mesh);
		rayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		rayInst.mask = 0xFF;
		rayInst.instanceShaderBindingTableRecordOffset = 0;  // We will use the same hit group for all objects
		tlas.emplace_back(rayInst);
	}
	vkb.build_tlas(tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void RTScene::create_rt_descriptors() {
	constexpr int TLAS_BINDING = 0;
	constexpr int IMAGE_BINDING = 1;
	constexpr int INSTANCE_BINDING = 2;

	std::vector<VkDescriptorPoolSize> pool_sizes = {
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1)
	};

	auto descriptor_pool_ci = vk::descriptor_pool_CI(
		pool_sizes.size(),
		pool_sizes.data(),
		2
	);
	vk::check(vkCreateDescriptorPool(vkb.ctx.device, &descriptor_pool_ci, nullptr, &rt_desc_pool),
			  "Failed to create RT descriptor pool");
	// RT bindings
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
			TLAS_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			IMAGE_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			INSTANCE_BINDING)
	};
	auto set_layout_ci = vk::descriptor_set_layout_CI(
		set_layout_bindings.data(),
		set_layout_bindings.size()
	);
	vk::check(vkCreateDescriptorSetLayout(vkb.ctx.device, &set_layout_ci, nullptr, &rt_desc_layout),
			  "Failed to create RT descriptor set layout");

	VkDescriptorSetAllocateInfo allocateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocateInfo.descriptorPool = rt_desc_pool;
	allocateInfo.descriptorSetCount = 1;
	allocateInfo.pSetLayouts = &rt_desc_layout;
	vkAllocateDescriptorSets(vkb.ctx.device, &allocateInfo, &rt_desc_set);

	VkAccelerationStructureKHR tlas = vkb.tlas.accel;
	VkWriteDescriptorSetAccelerationStructureKHR descASInfo{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	descASInfo.accelerationStructureCount = 1;
	descASInfo.pAccelerationStructures = &tlas;
	//VkDescriptorImageInfo imageInfo{ {}, offscreen_img.descriptor_image_info.imageView, VK_IMAGE_LAYOUT_GENERAL };

	// TODO: Abstraction
	std::vector<VkWriteDescriptorSet> writes{
			vk::write_descriptor_set(
				rt_desc_set,
				VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
				TLAS_BINDING,
				&descASInfo
			),
			vk::write_descriptor_set(
				rt_desc_set,
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				IMAGE_BINDING,
				&offscreen_img.descriptor_image_info
			),
			vk::write_descriptor_set(
				rt_desc_set,
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				INSTANCE_BINDING,
				&prim_lookup_buffer.descriptor
			)
	};
	vkUpdateDescriptorSets(vkb.ctx.device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}

void RTScene::create_rt_pipeline() {
	enum StageIndices
	{
		eRaygen,
		eMiss,
		eMiss2,
		eClosestHit,
		eShaderGroupCount
	};

	std::vector<Shader> shaders{
		{"src/shaders/raytrace.rgen"},
		{"src/shaders/raytrace.rmiss"},
		{"src/shaders/raytraceShadow.rmiss"},
		{"src/shaders/raytrace.rchit"}
	};
	for (auto& shader : shaders) {
		assert(shader.compile() == 0);
	}
	// All stages
	std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
	VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stage.pName = "main";  // All the same entry point
	// Raygen
	stage.module = shaders[eRaygen].create_vk_shader_module(vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	stages[eRaygen] = stage;
	// Miss
	stage.module = shaders[eMiss].create_vk_shader_module(vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[eMiss] = stage;
	// The second miss shader is invoked when a shadow ray misses the geometry. It simply indicates that no occlusion has been found
	stage.module = shaders[eMiss2].create_vk_shader_module(vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[eMiss2] = stage;
	// Hit Group - Closest Hit
	stage.module = shaders[eClosestHit].create_vk_shader_module(vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[eClosestHit] = stage;


	// Shader groups
	VkRayTracingShaderGroupCreateInfoKHR group{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
	group.anyHitShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = VK_SHADER_UNUSED_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.intersectionShader = VK_SHADER_UNUSED_KHR;

	// Raygen
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eRaygen;
	shader_groups.push_back(group);

	// Miss
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eMiss;
	shader_groups.push_back(group);

	// Shadow Miss
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = eMiss2;
	shader_groups.push_back(group);

	// closest hit shader
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = eClosestHit;
	shader_groups.push_back(group);

	// Push constant: we want to be able to update constants used by the shaders
	VkPushConstantRange pushConstant{ VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
									 0, sizeof(PushConstantRay) };


	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;

	// Descriptor sets: one specific to ray tracing, and one shared with the rasterization pipeline
	std::vector<VkDescriptorSetLayout> rtDescSetLayouts = { rt_desc_layout, uniform_set_layout };
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(rtDescSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = rtDescSetLayouts.data();

	vkCreatePipelineLayout(vkb.ctx.device, &pipelineLayoutCreateInfo, nullptr, &rt_pipeline_layout);


	// Assemble the shader stages and recursion depth info into the ray tracing pipeline
	VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
	rayPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());  // Stages are shaders
	rayPipelineInfo.pStages = stages.data();

	// In this case, m_rtShaderGroups.size() == 4: we have one raygen group,
	// two miss shader groups, and one hit group.
	rayPipelineInfo.groupCount = static_cast<uint32_t>(shader_groups.size());
	rayPipelineInfo.pGroups = shader_groups.data();

	// The ray tracing process can shoot rays from the camera, and a shadow ray can be shot from the
	// hit points of the camera rays, hence a recursion level of 2. This number should be kept as low
	// as possible for performance reasons. Even recursive ray tracing should be flattened into a loop
	// in the ray generation to avoid deep recursion.
	rayPipelineInfo.maxPipelineRayRecursionDepth = 2;  // Ray depth
	rayPipelineInfo.layout = rt_pipeline_layout;

	vkCreateRayTracingPipelinesKHR(vkb.ctx.device, {}, {}, 1, &rayPipelineInfo, nullptr, &rt_pipeline);

	// Spec only guarantees 1 level of "recursion". Check for that sad possibility here.
	if (rt_props.maxRayRecursionDepth <= 1) {
		throw std::runtime_error("Device fails to support ray recursion (m_rtProperties.maxRayRecursionDepth <= 1)");
	}

	for (auto& s : stages) {
		vkDestroyShaderModule(vkb.ctx.device, s.module, nullptr);
	}
}

//--------------------------------------------------------------------------------------------------
// The Shader Binding Table (SBT)
// - getting all shader handles and write them in a SBT buffer
// - Besides exception, this could be always done like this
//
void RTScene::create_rt_sbt() {
	uint32_t missCount{ 2 };
	uint32_t hitCount{ 1 };
	auto     handleCount = 1 + missCount + hitCount;
	uint32_t handleSize = rt_props.shaderGroupHandleSize;

	// The SBT (buffer) need to have starting groups to be aligned and handles in the group to be aligned.
	uint32_t handleSizeAligned = align_up(handleSize, rt_props.shaderGroupHandleAlignment);

	rgen_region.stride = align_up(handleSizeAligned, rt_props.shaderGroupBaseAlignment);
	rgen_region.size = rgen_region.stride;  // The size member of pRayGenShaderBindingTable must be equal to its stride member
	rmiss_region.stride = handleSizeAligned;
	rmiss_region.size = align_up(missCount * handleSizeAligned, rt_props.shaderGroupBaseAlignment);
	hit_region.stride = handleSizeAligned;
	hit_region.size = align_up(hitCount * handleSizeAligned, rt_props.shaderGroupBaseAlignment);

	// Get the shader group handles
	uint32_t             dataSize = handleCount * handleSize;
	std::vector<uint8_t> handles(dataSize);
	auto result = vkGetRayTracingShaderGroupHandlesKHR(vkb.ctx.device, rt_pipeline, 0, handleCount, dataSize, handles.data());
	assert(result == VK_SUCCESS);

	// Allocate a buffer for storing the SBT.
	VkDeviceSize sbtSize = rgen_region.size + rmiss_region.size + hit_region.size + call_region.size;
	sbt_buffer.create(&vkb.ctx,
					  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
					  | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
					  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					  VK_SHARING_MODE_EXCLUSIVE,
					  sbtSize
	);
	// Find the SBT addresses of each group
	VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, sbt_buffer.handle };
	VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(vkb.ctx.device, &info);
	rgen_region.deviceAddress = sbtAddress;
	rmiss_region.deviceAddress = sbtAddress + rgen_region.size;
	hit_region.deviceAddress = sbtAddress + rgen_region.size + rmiss_region.size;

	// Helper to retrieve the handle data
	auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

	// Map the SBT buffer and write in the handles.
	auto* pSBTBuffer = reinterpret_cast<uint8_t*>(sbt_buffer.data);
	uint8_t* pData{ nullptr };
	uint32_t handleIdx{ 0 };
	// Raygen
	pData = pSBTBuffer;
	memcpy(pData, getHandle(handleIdx++), handleSize);
	// Miss
	pData = pSBTBuffer + rgen_region.size;
	for (uint32_t c = 0; c < missCount; c++) {
		memcpy(pData, getHandle(handleIdx++), handleSize);
		pData += rmiss_region.stride;
	}
	// Hit
	pData = pSBTBuffer + rgen_region.size + rmiss_region.size;
	for (uint32_t c = 0; c < hitCount; c++) {
		memcpy(pData, getHandle(handleIdx++), handleSize);
		pData += hit_region.stride;
	}
	sbt_buffer.unmap();
}

void RTScene::create_offscreen_resources() {
	// Create offscreen image for output
	TextureSettings settings;
	settings.usage_flags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
		| VK_IMAGE_USAGE_STORAGE_BIT;
	settings.base_extent = { (uint32_t)width, (uint32_t)height, 1 };
	settings.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	offscreen_img.create_empty_texture(&vkb.ctx, settings, VK_IMAGE_LAYOUT_GENERAL);

	settings.usage_flags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	settings.format = VK_FORMAT_X8_D24_UNORM_PACK32;
	offscreen_depth.create_empty_texture(&vkb.ctx, settings, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_DEPTH_BIT);

	CommandBuffer cmd(&vkb.ctx, true);

	transition_image_layout(cmd.handle, offscreen_img.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	transition_image_layout(cmd.handle, offscreen_depth.img, VK_IMAGE_LAYOUT_UNDEFINED,
							VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
							VK_IMAGE_ASPECT_DEPTH_BIT
	);
	cmd.submit();

	offscreen_renderpass = create_render_pass(vkb.ctx.device, { VK_FORMAT_R32G32B32A32_SFLOAT }, VK_FORMAT_X8_D24_UNORM_PACK32, 1, true,
											  true, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
	std::vector<VkImageView> attachments = { offscreen_img.img_view, offscreen_depth.img_view };

	vkDestroyFramebuffer(vkb.ctx.device, offscreen_framebuffer, nullptr);
	VkFramebufferCreateInfo info{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
	info.renderPass = offscreen_renderpass;
	info.attachmentCount = 2;
	info.pAttachments = attachments.data();
	info.width = width;
	info.height = height;
	info.layers = 1;
	vkCreateFramebuffer(vkb.ctx.device, &info, nullptr, &offscreen_framebuffer);
}

void RTScene::create_post_descriptor() {
	constexpr int SAMPLER_COLOR_BINDING = 0;
	std::vector<VkDescriptorPoolSize> pool_sizes = {
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
	};
	auto descriptor_pool_ci = vk::descriptor_pool_CI(
		pool_sizes.size(),
		pool_sizes.data(),
		1
	);
	vk::check(vkCreateDescriptorPool(vkb.ctx.device, &descriptor_pool_ci, nullptr, &post_desc_pool),
			  "Failed to create descriptor pool");

	// Uniform buffer descriptors
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			SAMPLER_COLOR_BINDING),
	};
	auto set_layout_ci = vk::descriptor_set_layout_CI(
		set_layout_bindings.data(),
		set_layout_bindings.size()
	);
	vk::check(vkCreateDescriptorSetLayout(vkb.ctx.device, &set_layout_ci, nullptr, &post_desc_layout),
			  "Failed to create descriptor set layout");

	auto set_allocate_info = vk::descriptor_set_allocate_info(
		post_desc_pool,
		&post_desc_layout,
		1
	);
	vk::check(vkAllocateDescriptorSets(vkb.ctx.device, &set_allocate_info, &post_desc_set),
			  "Failed to allocate descriptor sets");
}

void RTScene::create_post_pipeline() {
	GraphicsPipelineSettings post_settings;
	VkPipelineLayoutCreateInfo create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

	create_info.setLayoutCount = 1;
	create_info.pSetLayouts = &post_desc_layout;
	vkCreatePipelineLayout(vkb.ctx.device, &create_info, nullptr, &post_pipeline_layout);

	post_settings.pipeline_layout = post_pipeline_layout;
	post_settings.render_pass = vkb.ctx.default_render_pass;
	post_settings.shaders = {
		{"src/shaders/post.vert"},
		{"src/shaders/post.frag"}
	};
	for (auto& shader : post_settings.shaders) {
		if (shader.compile()) {
			LUMEN_ERROR("Shader compilation failed");
		}
	}
	post_settings.cull_mode = VK_CULL_MODE_NONE;
	post_settings.enable_tracking = false;
	post_settings.dynamic_state_enables = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	post_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	post_pipeline->create_gfx_pipeline(post_settings);
}

void RTScene::update_post_desc_set() {
	auto write_desc_set = vk::write_descriptor_set(
		post_desc_set,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		0,
		&offscreen_img.descriptor_image_info
	);
	vkUpdateDescriptorSets(vkb.ctx.device, 1, &write_desc_set, 0, nullptr);
}

void RTScene::trace_rays() {
	// Initializing push constant values
	glm::vec4 clearColor{ 1,1,1,1 };
	pc_ray.clear_color = clearColor;
	pc_ray.light_pos = scene_ubo.light_pos;
	pc_ray.light_intensity = 10;
	CommandBuffer cmdBuf(&vkb.ctx);
	std::vector<VkDescriptorSet> descSets{ rt_desc_set, uniform_descriptor_sets[0] };
	vkCmdBindPipeline(cmdBuf.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline);
	vkCmdBindDescriptorSets(cmdBuf.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline_layout, 0,
							(uint32_t)descSets.size(), descSets.data(), 0, nullptr);
	vkCmdPushConstants(cmdBuf.handle, rt_pipeline_layout,
					   VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
					   0, sizeof(PushConstantRay), &pc_ray);
	vkCmdTraceRaysKHR(cmdBuf.handle, &rgen_region, &rmiss_region, &hit_region, &call_region, width, height, 1);
}


double RTScene::draw_frame() {
	auto t_begin = std::chrono::high_resolution_clock::now();
	vk::check(vkWaitForFences(vkb.ctx.device, 1, &vkb.in_flight_fences[vkb.current_frame], VK_TRUE, 1000000000),
			  "Timeout");
	uint32_t image_idx;
	VkResult result = vkAcquireNextImageKHR(
		vkb.ctx.device,
		vkb.ctx.swapchain,
		UINT64_MAX,
		vkb.image_available_sem[vkb.current_frame],
		VK_NULL_HANDLE,
		&image_idx);
	vk::check(vkResetCommandBuffer(vkb.ctx.command_buffers[image_idx], 0));
	if (result == VK_NOT_READY) {
		auto t_end = std::chrono::high_resolution_clock::now();
		auto t_diff = std::chrono::duration<double, std::milli>(t_end - t_begin).count();
		return t_diff;
	}
	if (EventHandler::consume_event(LumenEvent::EVENT_SHADER_RELOAD)) {
		// We don't want any command buffers in flight, might change in the future
		vkDeviceWaitIdle(vkb.ctx.device);
		for (auto& old_pipeline : EventHandler::obsolete_pipelines) {
			vkDestroyPipeline(vkb.ctx.device, old_pipeline, nullptr);
		}
		EventHandler::obsolete_pipelines.clear();
		vkb.create_command_buffers();
		auto t_end = std::chrono::high_resolution_clock::now();
		auto t_diff = std::chrono::duration<double, std::milli>(t_end - t_begin).count();
		return t_diff;
	}

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		// Window resize
		vkb.recreate_swap_chain(create_default_render_pass, vkb.ctx);
		auto t_end = std::chrono::high_resolution_clock::now();
		auto t_diff = std::chrono::duration<double, std::milli>(t_end - t_begin).count();
		return t_diff;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		LUMEN_ERROR("Failed to acquire new swap chain image");
	}

	if (vkb.images_in_flight[image_idx] != VK_NULL_HANDLE) {
		vkWaitForFences(vkb.ctx.device, 1, &vkb.images_in_flight[image_idx], VK_TRUE, UINT64_MAX);
	}
	vkb.images_in_flight[image_idx] = vkb.in_flight_fences[vkb.current_frame];

	render(image_idx);

	VkSubmitInfo submit_info = vk::submit_info();
	VkSemaphore wait_semaphores[] = { vkb.image_available_sem[vkb.current_frame] };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vkb.ctx.command_buffers[image_idx];

	VkSemaphore signal_semaphores[] = { vkb.render_finished_sem[vkb.current_frame] };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	vkResetFences(vkb.ctx.device, 1, &vkb.in_flight_fences[vkb.current_frame]);

	vk::check(vkQueueSubmit(vkb.ctx.queues[(int)QueueType::GFX], 1, &submit_info, vkb.in_flight_fences[vkb.current_frame]),
			  "Failed to submit draw command buffer"
	);
	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;

	VkSwapchainKHR swapchains[] = { vkb.ctx.swapchain };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;

	present_info.pImageIndices = &image_idx;

	result = vkQueuePresentKHR(vkb.ctx.queues[(int)QueueType::PRESENT], &present_info);

	if (result == VK_ERROR_OUT_OF_DATE_KHR ||
		result == VK_SUBOPTIMAL_KHR ||
		resized) {
		resized = false;
		vkb.recreate_swap_chain(create_default_render_pass, vkb.ctx);
	} else if (result != VK_SUCCESS) {
		LUMEN_ERROR("Failed to present swap chain image");
	}

	vkb.current_frame = (vkb.current_frame + 1) % vkb.MAX_FRAMES_IN_FLIGHT;
	auto t_end = std::chrono::high_resolution_clock::now();
	auto t_diff = std::chrono::duration<double, std::milli>(t_end - t_begin).count();
	return t_diff;
}

void RTScene::create_descriptors() {
	constexpr int UNIFORM_BUFFER_BINDING = 0;
	constexpr int SCENE_DESC_BINDING = 1;

	std::vector<VkDescriptorPoolSize> pool_sizes = {
	vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,vkb.ctx.swapchain_images.size()),
	vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<size_t>(gltf_scene.materials.size()) * 2),
	vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1),
	};
	auto descriptor_pool_ci = vk::descriptor_pool_CI(
		pool_sizes.size(),
		pool_sizes.data(),
		gltf_scene.materials.size() + vkb.ctx.swapchain_images.size() + 1
	);

	vk::check(vkCreateDescriptorPool(vkb.ctx.device, &descriptor_pool_ci, nullptr, &gfx_desc_pool),
			  "Failed to create descriptor pool");

	// Uniform buffer descriptors
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			UNIFORM_BUFFER_BINDING),
			vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
					  | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			SCENE_DESC_BINDING),
	};
	auto set_layout_ci = vk::descriptor_set_layout_CI(
		set_layout_bindings.data(),
		set_layout_bindings.size()
	);
	vk::check(vkCreateDescriptorSetLayout(vkb.ctx.device, &set_layout_ci, nullptr, &uniform_set_layout),
			  "Failed to create descriptor set layout");

	// Descriptor sets for the uniform matrices(For each swapchain image)
	auto set_layout_vec = std::vector<VkDescriptorSetLayout>(vkb.ctx.swapchain_images.size(), uniform_set_layout);
	auto set_allocate_info = vk::descriptor_set_allocate_info(
		gfx_desc_pool,
		set_layout_vec.data(),
		vkb.ctx.swapchain_images.size()
	);
	uniform_descriptor_sets.resize(vkb.ctx.swapchain_images.size());
	vk::check(vkAllocateDescriptorSets(vkb.ctx.device, &set_allocate_info, uniform_descriptor_sets.data()),
			  "Failed to allocate descriptor sets");

}

void RTScene::update_descriptors() {
	constexpr int UNIFORM_BUFFER_BINDING = 0;
	constexpr int SCENE_DESC_BINDING = 1;

	for (auto i = 0; i < uniform_descriptor_sets.size(); i++) {
		std::vector<VkWriteDescriptorSet> write_descriptor_sets{
			vk::write_descriptor_set(
				uniform_descriptor_sets[i],
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				UNIFORM_BUFFER_BINDING,
				&scene_ubo_buffer.descriptor
			),
			vk::write_descriptor_set(
				uniform_descriptor_sets[i],
				VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				SCENE_DESC_BINDING,
				&scene_desc_buffer.descriptor
			)
		};
		vkUpdateDescriptorSets(vkb.ctx.device,
							   static_cast<uint32_t>(write_descriptor_sets.size()),
							   write_descriptor_sets.data(), 0, nullptr);
	}
}

void RTScene::create_uniform_buffers() {
	scene_ubo_buffer.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		sizeof(SceneUBO));
	update_uniform_buffers();

}

void RTScene::create_graphics_pipeline() {
	// Need to specify push constant range
	auto model_push_constant_range = vk::push_constant_range(
		VK_SHADER_STAGE_VERTEX_BIT,
		sizeof(ModelPushConst), 0
	);

	auto material_push_constant_range = vk::push_constant_range(
		VK_SHADER_STAGE_FRAGMENT_BIT,
		sizeof(MaterialPushConst), sizeof(ModelPushConst)
	);

	std::array<VkDescriptorSetLayout, 1> set_layouts = { uniform_set_layout };
	auto pipeline_layout_ci = vk::pipeline_layout_CI(
		set_layouts.data(),
		static_cast<uint32_t>(set_layouts.size())
	);

	auto ranges = std::array<VkPushConstantRange, 2>{material_push_constant_range, model_push_constant_range};
	pipeline_layout_ci.pushConstantRangeCount = (uint32_t)ranges.size();
	pipeline_layout_ci.pPushConstantRanges = ranges.data();
	vk::check(vkCreatePipelineLayout(vkb.ctx.device, &pipeline_layout_ci, nullptr, &gfx_pipeline_layout),
			  "Failed to create pipeline layout");
	gfx_pipeline_settings.binding_desc = {
		{ 0, sizeof(glm::vec3) }, { 1, sizeof(glm::vec3) }, { 2, sizeof(glm::vec2) }
	};
	gfx_pipeline_settings.attribute_desc = {
		{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // Position
		{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0},  // Normal
		{2, 2, VK_FORMAT_R32G32_SFLOAT, 0},     // UVO
	};
	gfx_pipeline_settings.dynamic_state_enables = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	gfx_pipeline_settings.shaders = {
		{"src/shaders/gltf_simple.vert"},
		{"src/shaders/gltf_simple.frag"}
	};
	for (auto& shader : gfx_pipeline_settings.shaders) {
		if (shader.compile()) {
			LUMEN_ERROR("Shader compilation failed");
		}
	}
	gfx_pipeline_settings.name = "Cornell Box";
	gfx_pipeline_settings.render_pass = offscreen_renderpass;
	gfx_pipeline_settings.front_face = VK_FRONT_FACE_CLOCKWISE;
	gfx_pipeline_settings.enable_tracking = false;

	gfx_pipeline_settings.pipeline_layout = gfx_pipeline_layout;
	gfx_pipeline_settings.pipeline = {};

	gfx_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	gfx_pipeline->create_gfx_pipeline(gfx_pipeline_settings);
}

void RTScene::update_uniform_buffers() {
	camera->update_view_matrix();
	scene_ubo.view = camera->view;
	scene_ubo.projection = camera->projection;
	scene_ubo.view_pos = glm::vec4(camera->position, 1);
	scene_ubo.inv_view = glm::inverse(camera->view);
	scene_ubo.inv_projection = glm::inverse(camera->projection);
	scene_ubo.model = glm::mat4(1.0);
	scene_ubo.light_pos = glm::vec4(3.0f, 2.5f, 1.0f, 1.0f);
	memcpy(scene_ubo_buffer.data, &scene_ubo, sizeof(scene_ubo));
}

void RTScene::init_imgui() {
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	vk::check(vkCreateDescriptorPool(vkb.ctx.device, &pool_info, nullptr, &imgui_pool));
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	// Setup Platform/Renderer backends
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForVulkan(window->get_window_ptr(), true);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = vkb.ctx.instance;
	init_info.PhysicalDevice = vkb.ctx.physical_device;
	init_info.Device = vkb.ctx.device;
	init_info.Queue = vkb.ctx.queues[(int)QueueType::GFX];
	init_info.DescriptorPool = imgui_pool;
	init_info.MinImageCount = 2;
	init_info.ImageCount = 2;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info, vkb.ctx.default_render_pass);

	CommandBuffer cmd(&vkb.ctx, true);
	ImGui_ImplVulkan_CreateFontsTexture(cmd.handle);
	cmd.submit(vkb.ctx.queues[(int)QueueType::GFX]);
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void RTScene::render(uint32_t i) {
	auto cmdbuf = vkb.ctx.command_buffers[i];
	auto draw_gltf = [this](VkCommandBuffer cmd, VkPipelineLayout layout, VkPipeline pipeline) {
		std::vector<VkDeviceSize> offsets = { 0, 0, 0 };
		std::vector<VkBuffer> buffers = { vertex_buffer.handle, normal_buffer.handle, uv_buffer.handle };

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gfx_pipeline->handle);
		vkCmdBindVertexBuffers(cmd, 0, static_cast<uint32_t>(buffers.size()), buffers.data(), offsets.data());
		vkCmdBindIndexBuffer(cmd, index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
		uint32_t idx_node = 0;
		for (auto& node : gltf_scene.nodes) {
			auto& primitive = gltf_scene.prim_meshes[node.prim_mesh];
			auto& material = gltf_scene.materials[primitive.material_idx];
			vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
							   sizeof(glm::mat4), &node.world_matrix);
			material_push_const.base_color_factor = material.base_color_factor;
			vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_FRAGMENT_BIT,
							   sizeof(ModelPushConst), sizeof(MaterialPushConst), &material_push_const
			);
			vkCmdDrawIndexed(cmd, primitive.idx_count, 1, primitive.first_idx, primitive.vtx_offset, 0);
		}
	};

	VkCommandBufferBeginInfo begin_info = vk::command_buffer_begin_info(
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	);
	vk::check(vkBeginCommandBuffer(cmdbuf, &begin_info));
	VkClearValue clear_color = { 0.25f, 0.25f, 0.25f, 1.0f };
	VkClearValue clear_depth = { 1.0f, 0 };
	VkViewport viewport = vk::viewport((float)width, (float)height, 0.0f, 1.0f);
	VkClearValue clear_values[] = { clear_color, clear_depth };
	// 1st pass
	if (USE_RTX) {
		// Initializing push constant values
		glm::vec4 clearColor{ 1,1,1,1 };
		pc_ray.clear_color = clearColor;
		pc_ray.light_pos = scene_ubo.light_pos;
		pc_ray.light_type = 0;
		pc_ray.light_intensity = 10;
		std::vector<VkDescriptorSet> descSets{ rt_desc_set, uniform_descriptor_sets[0] };
		vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline);
		vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rt_pipeline_layout, 0,
								(uint32_t)descSets.size(), descSets.data(), 0, nullptr);
		vkCmdPushConstants(cmdbuf, rt_pipeline_layout,
						   VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
						   0, sizeof(PushConstantRay), &pc_ray);
		vkCmdTraceRaysKHR(cmdbuf, &rgen_region, &rmiss_region, &hit_region, &call_region, width, height, 1);

	} else {
		VkRenderPassBeginInfo gfx_rpi = vk::render_pass_begin_info();
		gfx_rpi.renderPass = offscreen_renderpass;
		gfx_rpi.framebuffer = offscreen_framebuffer;
		gfx_rpi.renderArea = { { 0, 0 }, vkb.ctx.swapchain_extent };
		gfx_rpi.clearValueCount = 2;
		gfx_rpi.pClearValues = &clear_values[0];

		vkCmdBeginRenderPass(cmdbuf, &gfx_rpi, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

		VkRect2D scissor = vk::rect2D(width, height, 0, 0);
		vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

		vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
								gfx_pipeline_layout, 0, 1, &uniform_descriptor_sets[i], 0, nullptr);
		draw_gltf(cmdbuf, gfx_pipeline_layout, gfx_pipeline->handle);

		vkCmdEndRenderPass(cmdbuf);
	}
	// 2nd pass
	VkRenderPassBeginInfo post_rpi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	post_rpi.clearValueCount = 2;
	post_rpi.pClearValues = clear_values;
	post_rpi.renderPass = vkb.ctx.default_render_pass;
	post_rpi.framebuffer = vkb.ctx.swapchain_framebuffers[i];
	post_rpi.renderArea = { {0, 0},  vkb.ctx.swapchain_extent };

	vkCmdBeginRenderPass(cmdbuf, &post_rpi, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
	VkRect2D scissor = vk::rect2D(width, height, 0, 0);
	vkCmdSetScissor(cmdbuf, 0, 1, &scissor);
	vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, post_pipeline->handle);
	vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, post_pipeline_layout, 0, 1, &post_desc_set, 0, nullptr);
	vkCmdDraw(cmdbuf, 3, 1, 0, 0);
	vkCmdEndRenderPass(cmdbuf);


	//ImGui_ImplVulkan_NewFrame();
	//ImGui_ImplGlfw_NewFrame();
	//ImGui::NewFrame();
	//bool open = true;
	//ImGui::ShowDemoWindow(&open);
	//ImGui::Render();
	//ImDrawData* draw_data = ImGui::GetDrawData();
	//const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
	//if (is_minimized) {
	//	return;
	//}
	//ImGui_ImplVulkan_RenderDrawData(draw_data, cmdbuf);


	vk::check(vkEndCommandBuffer(cmdbuf),
			  "Failed to record command buffer"
	);
}

void RTScene::update() {

	double frame_time = draw_frame();
	frame_time /= 1000.0;
	glm::vec3 translation{};
	float trans_speed = 0.001f;
	glm::vec3 front;
	if (window->is_key_held(KeyInput::KEY_LEFT_SHIFT)) {
		trans_speed *= 4;
	}
	front.x = cos(glm::radians(camera->rotation.x)) * sin(glm::radians(camera->rotation.y));
	front.y = sin(glm::radians(camera->rotation.x));
	front.z = cos(glm::radians(camera->rotation.x)) * cos(glm::radians(camera->rotation.y));
	front = glm::normalize(-front);
	if (window->is_key_held(KeyInput::KEY_W)) {
		camera->position += front * trans_speed;
	}
	if (window->is_key_held(KeyInput::KEY_A)) {
		camera->position -= glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * trans_speed;
	}
	if (window->is_key_held(KeyInput::KEY_S)) {
		camera->position -= front * trans_speed;
	}
	if (window->is_key_held(KeyInput::KEY_D)) {
		camera->position += glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * trans_speed;
	}
	if (window->is_key_held(KeyInput::SPACE)) {
		// Right
		auto right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
		auto up = glm::cross(right, front);
		camera->position += up * trans_speed;
	}
	if (window->is_key_held(KeyInput::KEY_LEFT_CONTROL)) {
		auto right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
		auto up = glm::cross(right, front);
		camera->position -= up * trans_speed;
	}
	update_uniform_buffers();
}

void RTScene::cleanup() {
	auto device = vkb.ctx.device;
	vkDeviceWaitIdle(device);
	std::vector<Buffer> buffer_list = {
		vertex_buffer,
		normal_buffer,
		uv_buffer,
		index_buffer,
		materials_buffer,
		prim_lookup_buffer,
		scene_desc_buffer,
		scene_ubo_buffer
	};
	if (USE_RTX) {
		buffer_list.push_back(sbt_buffer);
	}
	for (auto& b : buffer_list) {
		b.destroy();
	}
	offscreen_img.destroy();
	offscreen_depth.destroy();

	vkDestroyDescriptorSetLayout(device, uniform_set_layout, nullptr);
	vkDestroyDescriptorSetLayout(device, post_desc_layout, nullptr);

	vkDestroyDescriptorPool(device, gfx_desc_pool, nullptr);
	vkDestroyDescriptorPool(device, post_desc_pool, nullptr);

	vkDestroyPipelineLayout(device, gfx_pipeline_layout, nullptr);
	vkDestroyPipelineLayout(device, post_pipeline_layout, nullptr);

	vkDestroyRenderPass(device, offscreen_renderpass, nullptr);
	vkDestroyFramebuffer(device, offscreen_framebuffer, nullptr);

	if (gfx_pipeline) { gfx_pipeline->cleanup(); }
	if (post_pipeline) { post_pipeline->cleanup(); }

	if (USE_RTX) {
		vkDestroyPipeline(vkb.ctx.device, rt_pipeline, nullptr);
		vkDestroyDescriptorSetLayout(device, rt_desc_layout, nullptr);
		vkDestroyDescriptorPool(device, rt_desc_pool, nullptr);
		vkDestroyPipelineLayout(device, rt_pipeline_layout, nullptr);
	}
	if (initialized) { vkb.cleanup(); }
}
