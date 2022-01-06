#include "LumenPCH.h"
#include "VCM.h"

const int max_depth = 6;
const vec3 sky_col(0, 0, 0);
static float vcm_radius_factor = 0.1f;
static bool use_vm = false;
static bool use_vc = true;
void VCM::init() {
	Integrator::init();

	photon_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		10 * instance->width * instance->height * sizeof(PhotonHash)
	);

	vcm_light_vertices_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		instance->width * instance->height * (max_depth + 1) * sizeof(VCMVertex)
	);

	light_path_cnt_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		instance->width * instance->height * sizeof(float)
	);

	color_storage_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		instance->width * instance->height * 3 * sizeof(float)
	);

	SceneDesc desc;
	desc.vertex_addr = vertex_buffer.get_device_address();
	desc.index_addr = index_buffer.get_device_address();
	desc.normal_addr = normal_buffer.get_device_address();
	desc.uv_addr = uv_buffer.get_device_address();
	desc.material_addr = materials_buffer.get_device_address();
	desc.prim_info_addr = prim_lookup_buffer.get_device_address();
	// VCM
	desc.photon_addr = photon_buffer.get_device_address();
	desc.vcm_vertices_addr = vcm_light_vertices_buffer.get_device_address();
	desc.path_cnt_addr = light_path_cnt_buffer.get_device_address();
	desc.color_storage_addr = color_storage_buffer.get_device_address();

	scene_desc_buffer.create(&instance->vkb.ctx,
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
							 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							 VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneDesc),
							 &desc, true);
	create_blas();
	create_tlas();
	create_offscreen_resources();
	create_descriptors();
	create_rt_pipelines();
	pc_ray.total_light_area = 0;
	pc_ray.frame_num = 0;
	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;
}

void VCM::render() {
	const float ppm_base_radius = 0.25f;
	CommandBuffer cmd(&instance->vkb.ctx, /*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VkClearValue clear_color = { 0.25f, 0.25f, 0.25f, 1.0f };
	VkClearValue clear_depth = { 1.0f, 0 };
	VkViewport viewport = vk::viewport((float)instance->width, (float)instance->height, 0.0f, 1.0f);
	VkClearValue clear_values[] = { clear_color, clear_depth };
	pc_ray.light_pos = scene_ubo.light_pos;
	pc_ray.light_type = 0;
	pc_ray.light_intensity = 10;
	pc_ray.num_mesh_lights = int(lights.size());
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = max_depth;
	pc_ray.sky_col = sky_col;
	// VCM related constants
	pc_ray.radius = lumen_scene.m_dimensions.radius * vcm_radius_factor / 100.f;
	pc_ray.radius /= (float)pow((double)pc_ray.frame_num + 1, 0.5 * (1 - 2.0 / 3));
	pc_ray.min_bounds = lumen_scene.m_dimensions.min;
	pc_ray.max_bounds = lumen_scene.m_dimensions.max;
	pc_ray.ppm_base_radius = ppm_base_radius;
	pc_ray.use_vm = use_vm;
	pc_ray.use_vc = use_vc;
	const glm::vec3 diam = pc_ray.max_bounds - pc_ray.min_bounds;
	const float max_comp = glm::max(diam.x, glm::max(diam.y, diam.z));
	const int base_grid_res = int(max_comp / pc_ray.radius);
	pc_ray.grid_res = glm::max(ivec3(diam * float(base_grid_res) / max_comp), ivec3(1));

	// Prepare
	{
		if (use_vm) {
			vkCmdFillBuffer(cmd.handle, photon_buffer.handle, 0, photon_buffer.size, 0);
			auto barrier = buffer_barrier(photon_buffer.handle,
										  VK_ACCESS_TRANSFER_WRITE_BIT,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &barrier, 0, 0);
		}

	}
	// Trace rays from light
	{
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
						  vcm_light_pipeline->handle);
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vcm_light_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdPushConstants(cmd.handle, vcm_light_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_RAYGEN_BIT_KHR |
						   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
						   VK_SHADER_STAGE_MISS_BIT_KHR,
						   0, sizeof(PushConstantRay), &pc_ray);
		auto& regions = vcm_light_pipeline->get_rt_regions();
		vkCmdTraceRaysKHR(cmd.handle, &regions[0], &regions[1], &regions[2], 
						  &regions[3], instance->width, instance->height, 1);
	}
	// Trace rays from eye
	{
		if (use_vm) {
			auto barrier = buffer_barrier(photon_buffer.handle,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &barrier, 0, 0);
		}
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
						  vcm_eye_pipeline->handle);
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vcm_eye_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdPushConstants(cmd.handle, vcm_eye_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_RAYGEN_BIT_KHR |
						   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
						   VK_SHADER_STAGE_MISS_BIT_KHR,
						   0, sizeof(PushConstantRay), &pc_ray);
		auto& regions = vcm_eye_pipeline->get_rt_regions();
		vkCmdTraceRaysKHR(cmd.handle, &regions[0], &regions[1], &regions[2], &regions[3], 
						  instance->width, instance->height, 1);
	}
	cmd.submit();
}

bool VCM::update() {
	pc_ray.frame_num++;
	glm::vec3 translation{};
	float trans_speed = 0.01f;
	glm::vec3 front;
	if (instance->window->is_key_held(KeyInput::KEY_LEFT_SHIFT)) {
		trans_speed *= 4;
	}

	front.x = cos(glm::radians(camera->rotation.x)) *
		sin(glm::radians(camera->rotation.y));
	front.y = sin(glm::radians(camera->rotation.x));
	front.z = cos(glm::radians(camera->rotation.x)) *
		cos(glm::radians(camera->rotation.y));
	front = glm::normalize(-front);
	if (instance->window->is_key_held(KeyInput::KEY_W)) {
		camera->position += front * trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::KEY_A)) {
		camera->position -=
			glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) *
			trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::KEY_S)) {
		camera->position -= front * trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::KEY_D)) {
		camera->position +=
			glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) *
			trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::SPACE)) {
		// Right
		auto right =
			glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
		auto up = glm::cross(right, front);
		camera->position += up * trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::KEY_LEFT_CONTROL)) {
		auto right =
			glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
		auto up = glm::cross(right, front);
		camera->position -= up * trans_speed;
		updated = true;
	}
	bool result = false;
	if (updated) {
		result = true;
		pc_ray.frame_num = 0;
		updated = false;
	}
	update_uniform_buffers();
	return result;
}

void VCM::create_offscreen_resources() {
	// Create offscreen image for output
	TextureSettings settings;
	settings.usage_flags =
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	settings.base_extent = { (uint32_t)instance->width, (uint32_t)instance->height, 1 };
	settings.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	output_tex.create_empty_texture(&instance->vkb.ctx, settings,
									VK_IMAGE_LAYOUT_GENERAL);
	CommandBuffer cmd(&instance->vkb.ctx, true);
	transition_image_layout(cmd.handle, output_tex.img,
							VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	cmd.submit();
}

void VCM::create_descriptors() {
	constexpr int TLAS_BINDING = 0;
	constexpr int IMAGE_BINDING = 1;
	constexpr int INSTANCE_BINDING = 2;
	constexpr int UNIFORM_BUFFER_BINDING = 3;
	constexpr int SCENE_DESC_BINDING = 4;
	constexpr int TEXTURES_BINDING = 5;
	constexpr int LIGHTS_BINDING = 6;

	auto num_textures = textures.size();
	std::vector<VkDescriptorPoolSize> pool_sizes = {
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
								 num_textures),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
								 1),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1) };
	auto descriptor_pool_ci =
		vk::descriptor_pool_CI(pool_sizes.size(), pool_sizes.data(),
							   instance->vkb.ctx.swapchain_images.size());

	vk::check(vkCreateDescriptorPool(instance->vkb.ctx.device, &descriptor_pool_ci,
			  nullptr, &desc_pool),
			  "Failed to create descriptor pool");

	// Uniform buffer descriptors
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
			TLAS_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_COMPUTE_BIT,
			IMAGE_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
				VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			INSTANCE_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT |
				VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			UNIFORM_BUFFER_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
				VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
			SCENE_DESC_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
				VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			TEXTURES_BINDING, num_textures),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			LIGHTS_BINDING)
	};
	auto set_layout_ci = vk::descriptor_set_layout_CI(
		set_layout_bindings.data(), set_layout_bindings.size());
	vk::check(vkCreateDescriptorSetLayout(instance->vkb.ctx.device, &set_layout_ci,
			  nullptr, &desc_set_layout),
			  "Failed to create escriptor set layout");
	VkDescriptorSetAllocateInfo set_allocate_info{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	set_allocate_info.descriptorPool = desc_pool;
	set_allocate_info.descriptorSetCount = 1;
	set_allocate_info.pSetLayouts = &desc_set_layout;
	vkAllocateDescriptorSets(instance->vkb.ctx.device, &set_allocate_info, &desc_set);

	// Update descriptors
	VkAccelerationStructureKHR tlas = instance->vkb.tlas.accel;
	VkWriteDescriptorSetAccelerationStructureKHR desc_as_info{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	desc_as_info.accelerationStructureCount = 1;
	desc_as_info.pAccelerationStructures = &tlas;

	// TODO: Abstraction
	std::vector<VkWriteDescriptorSet> writes{
		vk::write_descriptor_set(desc_set,
								 VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
								 TLAS_BINDING, &desc_as_info),
		vk::write_descriptor_set(desc_set, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
								 IMAGE_BINDING,
								 &output_tex.descriptor_image_info),
		vk::write_descriptor_set(desc_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
								 INSTANCE_BINDING,
								 &prim_lookup_buffer.descriptor),
	   vk::write_descriptor_set(desc_set,
								VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
								UNIFORM_BUFFER_BINDING, &scene_ubo_buffer.descriptor),
	   vk::write_descriptor_set(desc_set,
								VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
								SCENE_DESC_BINDING, &scene_desc_buffer.descriptor) };
	std::vector<VkDescriptorImageInfo> image_infos;
	for (auto& tex : textures) {
		image_infos.push_back(tex.descriptor_image_info);
	}
	writes.push_back(vk::write_descriptor_set(desc_set,
					 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, TEXTURES_BINDING,
					 image_infos.data(), (uint32_t)image_infos.size()));
	if (lights.size()) {
		writes.push_back(vk::write_descriptor_set(
			desc_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, LIGHTS_BINDING,
			&mesh_lights_buffer.descriptor));
	}
	vkUpdateDescriptorSets(
		instance->vkb.ctx.device, static_cast<uint32_t>(writes.size()),
		writes.data(), 0, nullptr);
};


void VCM::create_blas() {
	std::vector<BlasInput> blas_inputs;
	auto vertex_address =
		get_device_address(instance->vkb.ctx.device, vertex_buffer.handle);
	auto idx_address = get_device_address(instance->vkb.ctx.device, index_buffer.handle);
	for (auto& prim_mesh : lumen_scene.prim_meshes) {
		BlasInput geo = to_vk_geometry(prim_mesh, vertex_address, idx_address);
		blas_inputs.push_back({ geo });
	}
	instance->vkb.build_blas(blas_inputs,
						  VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void VCM::create_tlas() {
	std::vector<VkAccelerationStructureInstanceKHR> tlas;
	float total_light_triangle_area = 0.0f;
	int light_triangle_cnt = 0;
	const auto& indices = lumen_scene.indices;
	const auto& vertices = lumen_scene.positions;
	for (const auto& pm : lumen_scene.prim_meshes) {
		VkAccelerationStructureInstanceKHR ray_inst{};
		ray_inst.transform = to_vk_matrix(pm.world_matrix);
		ray_inst.instanceCustomIndex = pm.prim_idx;
		ray_inst.accelerationStructureReference =
			instance->vkb.get_blas_device_address(pm.prim_idx);
		ray_inst.flags =
			VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		ray_inst.mask = 0xFF;
		ray_inst.instanceShaderBindingTableRecordOffset =
			0; // We will use the same hit group for all objects
		tlas.emplace_back(ray_inst);
	}

	for (auto& l : lights) {
		const auto& pm = lumen_scene.prim_meshes[l.prim_mesh_idx];
		l.world_matrix = pm.world_matrix;
		auto& idx_base_offset = pm.first_idx;
		auto& vtx_offset = pm.vtx_offset;
		for (uint32_t i = 0; i < l.num_triangles; i++) {
			auto idx_offset = idx_base_offset + 3 * i;
			glm::ivec3 ind = { indices[idx_offset], indices[idx_offset + 1],
							  indices[idx_offset + 2] };
			ind += glm::vec3{ vtx_offset, vtx_offset, vtx_offset };
			const vec3 v0 =
				pm.world_matrix * glm::vec4(vertices[ind.x], 1.0);
			const vec3 v1 =
				pm.world_matrix * glm::vec4(vertices[ind.y], 1.0);
			const vec3 v2 =
				pm.world_matrix * glm::vec4(vertices[ind.z], 1.0);
			float area = 0.5f * glm::length(glm::cross(v1 - v0, v2 - v0));
			total_light_triangle_area += area;
		}
		light_triangle_cnt += l.num_triangles;
	}

	if (lights.size()) {
		mesh_lights_buffer.create(
			&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
			lights.size() * sizeof(MeshLight), lights.data(), true);
	}

	pc_ray.total_light_area += total_light_triangle_area;
	if (light_triangle_cnt > 0) {
		pc_ray.light_triangle_count = light_triangle_cnt;
	}
	instance->vkb.build_tlas(tlas,
						  VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void VCM::create_rt_pipelines() {
	enum StageIndices {
		Raygen = 0,
		CMiss = 1,
		AMiss = 2,
		ClosestHit = 3,
		AnyHit = 4,
		ShaderGroupCount = 5
	};
	RTPipelineSettings settings;

	std::vector<Shader> shaders{ {"src/shaders/integrators/vcm/vcm_light.rgen"},
								{"src/shaders/integrators/vcm/vcm_eye.rgen"},
								{"src/shaders/integrators/ray.rmiss"},
								{"src/shaders/integrators/ray_shadow.rmiss"},
								{"src/shaders/integrators/ray.rchit"},
								{"src/shaders/integrators/ray.rahit"} };
	for (auto& shader : shaders) {
		shader.compile();
	}
	settings.ctx = &instance->vkb.ctx;
	settings.rt_props = rt_props;
	// All stages

	std::vector<VkPipelineShaderStageCreateInfo> stages;
	stages.resize(ShaderGroupCount);

	VkPipelineShaderStageCreateInfo stage{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stage.pName = "main";
	// Raygen
	stage.module = shaders[0].create_vk_shader_module(instance->vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	stages[Raygen] = stage;
	// Miss
	stage.module = shaders[2].create_vk_shader_module(instance->vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[CMiss] = stage;

	stage.module = shaders[3].create_vk_shader_module(instance->vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[AMiss] = stage;
	// Hit Group - Closest Hit
	stage.module = shaders[4].create_vk_shader_module(instance->vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[ClosestHit] = stage;
	// Hit Group - Any hit
	stage.module = shaders[5].create_vk_shader_module(instance->vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	stages[AnyHit] = stage;


	// Shader groups
	VkRayTracingShaderGroupCreateInfoKHR group{
		VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
	group.anyHitShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = VK_SHADER_UNUSED_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.intersectionShader = VK_SHADER_UNUSED_KHR;

	// Raygen
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = Raygen;
	settings.groups.push_back(group);

	// Miss
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = CMiss;
	settings.groups.push_back(group);

	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = AMiss;
	settings.groups.push_back(group);

	// closest hit shader
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = ClosestHit;
	settings.groups.push_back(group);

	// Any hit shader
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.anyHitShader = AnyHit;
	settings.groups.push_back(group);

	settings.push_consts.push_back({ VK_SHADER_STAGE_RAYGEN_BIT_KHR |
										 VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
										 VK_SHADER_STAGE_MISS_BIT_KHR,
									 0, sizeof(PushConstantRay) });
	settings.desc_layouts = { desc_set_layout };
	settings.stages = stages;
	vcm_light_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	vcm_eye_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	settings.shaders = { shaders[0], shaders[2], shaders[3], shaders[4], shaders[5] };
	vcm_light_pipeline->create_rt_pipeline(settings);
	vkDestroyShaderModule(instance->vkb.ctx.device, stages[Raygen].module, nullptr);
	stages[Raygen].module = shaders[1].create_vk_shader_module(instance->vkb.ctx.device);
	settings.stages = stages;
	settings.shaders = { shaders[1], shaders[2], shaders[3], shaders[4], shaders[5] };
	vcm_eye_pipeline->create_rt_pipeline(settings);
	for (auto& s : settings.stages) {
		vkDestroyShaderModule(instance->vkb.ctx.device, s.module, nullptr);
	}
}

void VCM::destroy() {
	const auto device = instance->vkb.ctx.device;
	Integrator::destroy();
	std::vector<Buffer*> buffer_list = {
		&photon_buffer,
		&vcm_light_vertices_buffer,
		&light_path_cnt_buffer,
		&color_storage_buffer
	};
	for (auto b : buffer_list) {
		b->destroy();
	}
	std::vector<Pipeline*> pipeline_list = {
		vcm_light_pipeline.get(),
		vcm_eye_pipeline.get()
	};
	for (auto p : pipeline_list) {
		p->cleanup();
	}
	vkDestroyDescriptorSetLayout(device, desc_set_layout, nullptr);
	vkDestroyDescriptorPool(device, desc_pool, nullptr);
}

void VCM::reload() {
	vcm_light_pipeline->reload();
	vcm_eye_pipeline->reload();
}
