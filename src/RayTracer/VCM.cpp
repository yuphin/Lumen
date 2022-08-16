#include "LumenPCH.h"
#include "VCM.h"
#include <iostream>
#include <fstream>
const int max_samples = 50000;
static bool use_vc = true;
static bool written = false;
const bool ray_guide = false;
void VCM::init() {
	Integrator::init();
	photon_buffer.create("Photon Buffer", &instance->vkb.ctx,
						 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
						 10 * instance->width * instance->height * sizeof(PhotonHash));

	vcm_light_vertices_buffer.create(
		"VCM Light Vertices", &instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		instance->width * instance->height * (lumen_scene->config.path_length + 1) * sizeof(VCMVertex));

	light_path_cnt_buffer.create("Light Path Count", &instance->vkb.ctx,
								 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
								 instance->width * instance->height * sizeof(float));

	color_storage_buffer.create("Color Storage", &instance->vkb.ctx,
								VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
								instance->width * instance->height * 3 * sizeof(float));

	vcm_reservoir_buffer.create("VCM Reservoirs", &instance->vkb.ctx,
								VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
								instance->width * instance->height * sizeof(VCMReservoir));

	light_samples_buffer.create("Light Samples", &instance->vkb.ctx,
								VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
								instance->width * instance->height * sizeof(VCMRestirData));

	should_resample_buffer.create("Should Resample", &instance->vkb.ctx,
								  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, 4);

	light_state_buffer.create("Light States", &instance->vkb.ctx,
							  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
							  instance->width * instance->height * sizeof(LightState));

	angle_struct_buffer.create("Angle Struct", &instance->vkb.ctx,
							   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
							   max_samples * sizeof(AngleStruct));

	angle_struct_cpu_buffer.create(&instance->vkb.ctx,
								   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
									   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
								   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								   VK_SHARING_MODE_EXCLUSIVE, max_samples * sizeof(AngleStruct));

	avg_buffer.create("Average", &instance->vkb.ctx,
					  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
						  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					  VK_SHARING_MODE_EXCLUSIVE, sizeof(AvgStruct));

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

	desc.vcm_reservoir_addr = vcm_reservoir_buffer.get_device_address();
	desc.light_samples_addr = light_samples_buffer.get_device_address();
	desc.should_resample_addr = should_resample_buffer.get_device_address();
	desc.light_state_addr = light_state_buffer.get_device_address();
	desc.angle_struct_addr = angle_struct_buffer.get_device_address();
	desc.avg_addr = avg_buffer.get_device_address();

	scene_desc_buffer.create(
		&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneDesc), &desc, true);
	pc_ray.total_light_area = 0;
	pc_ray.frame_num = 0;
	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;

	assert(instance->vkb.rg->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, photon_addr, &photon_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, vcm_vertices_addr, &vcm_light_vertices_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, path_cnt_addr, &light_path_cnt_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, &color_storage_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, vcm_reservoir_addr, &vcm_reservoir_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_samples_addr, &light_samples_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, should_resample_addr, &should_resample_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_state_addr, &light_state_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, angle_struct_addr, &angle_struct_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, avg_addr, &avg_buffer, instance->vkb.rg);
}

void VCM::render() {
	const float ppm_base_radius = 0.25f;
	// CommandBuffer cmd(&instance->vkb.ctx, /*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	// VkClearValue clear_color = {0.25f, 0.25f, 0.25f, 1.0f};
	// VkClearValue clear_depth = {1.0f, 0};
	// VkViewport viewport = vk::viewport((float)instance->width, (float)instance->height, 0.0f, 1.0f);
	// VkClearValue clear_values[] = {clear_color, clear_depth};
	pc_ray.light_pos = scene_ubo.light_pos;
	pc_ray.light_type = 0;
	pc_ray.light_intensity = 10;
	pc_ray.num_lights = int(lights.size());
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = lumen_scene->config.path_length;
	pc_ray.sky_col = lumen_scene->config.sky_col;
	// VCM related constants
	pc_ray.radius = lumen_scene->m_dimensions.radius * lumen_scene->config.radius_factor / 100.f;
	pc_ray.radius /= (float)pow((double)pc_ray.frame_num + 1, 0.5 * (1 - 2.0 / 3));
	pc_ray.min_bounds = lumen_scene->m_dimensions.min;
	pc_ray.max_bounds = lumen_scene->m_dimensions.max;
	pc_ray.ppm_base_radius = ppm_base_radius;
	pc_ray.use_vm = lumen_scene->config.enable_vm;
	pc_ray.use_vc = use_vc;
	pc_ray.do_spatiotemporal = do_spatiotemporal;
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.max_angle_samples = max_samples;
	pc_ray.light_triangle_count = total_light_triangle_cnt;
	const glm::vec3 diam = pc_ray.max_bounds - pc_ray.min_bounds;
	const float max_comp = glm::max(diam.x, glm::max(diam.y, diam.z));
	const int base_grid_res = int(max_comp / pc_ray.radius);
	pc_ray.grid_res = glm::max(ivec3(diam * float(base_grid_res) / max_comp), ivec3(1));
	// Prepare
	auto& prepare_pass =
		instance->vkb.rg
			->add_compute("Init Reservoirs",
						  {.shader = Shader("src/shaders/integrators/vcm/init_reservoirs.comp"),
						   .dims = {(uint32_t)std::ceil(instance->width * instance->height / float(1024.0f)), 1, 1}})
			.push_constants(&pc_ray)
			.bind(scene_desc_buffer)
			.zero(photon_buffer, lumen_scene->config.enable_vm);

	if (!do_spatiotemporal) {
		prepare_pass.zero({light_samples_buffer, should_resample_buffer});
	} else {
		prepare_pass.skip_execution();
	}

	// Do resampling
	instance->vkb.rg
		->add_rt("Resample", {.shaders = {{"src/shaders/integrators/vcm/vcm_sample.rgen"},
										  {"src/shaders/ray.rmiss"},
										  {"src/shaders/ray_shadow.rmiss"},
										  {"src/shaders/ray.rchit"},
										  {"src/shaders/ray.rahit"}},
							  .dims = {instance->width, instance->height},
							  .accel = instance->vkb.tlas.accel})
		.push_constants(&pc_ray)
		.bind({
			output_tex,
			prim_lookup_buffer,
			scene_ubo_buffer,
			scene_desc_buffer,
		})
		.bind_texture_array(diffuse_textures)
		.bind(mesh_lights_buffer)
		.bind_tlas(instance->vkb.tlas);

	// Check resampling
	instance->vkb.rg
		->add_compute("Check Reservoirs",
					  {.shader = Shader("src/shaders/integrators/vcm/check_reservoirs.comp"),
					   .dims = {(uint32_t)std::ceil(instance->width * instance->height / float(1024.0f)), 1, 1}})
		.push_constants(&pc_ray)
		.bind(scene_desc_buffer)
		.zero(should_resample_buffer);
	pc_ray.random_num = rand() % UINT_MAX;
	// Spawn light rays
	instance->vkb.rg
		->add_rt("VCM - Spawn Light", {.shaders = {{"src/shaders/integrators/vcm/vcm_spawn_light.rgen"},
												   {"src/shaders/ray.rmiss"},
												   {"src/shaders/ray_shadow.rmiss"},
												   {"src/shaders/ray.rchit"},
												   {"src/shaders/ray.rahit"}},
									   .dims = {instance->width, instance->height},
									   .accel = instance->vkb.tlas.accel})
		.push_constants(&pc_ray)
		.zero(light_state_buffer)
		.bind({
			output_tex,
			prim_lookup_buffer,
			scene_ubo_buffer,
			scene_desc_buffer,
		})
		.bind_texture_array(diffuse_textures)
		.bind(mesh_lights_buffer)
		.bind_tlas(instance->vkb.tlas);
	pc_ray.random_num = rand() % UINT_MAX;
	// Trace spawned rays
	instance->vkb.rg
		->add_rt("VCM - Trace Light", {.shaders = {{"src/shaders/integrators/vcm/vcm_light.rgen"},
												   {"src/shaders/ray.rmiss"},
												   {"src/shaders/ray_shadow.rmiss"},
												   {"src/shaders/ray.rchit"},
												   {"src/shaders/ray.rahit"}},
									   .dims = {instance->width, instance->height},
									   .accel = instance->vkb.tlas.accel})
		.push_constants(&pc_ray)
		.bind({
			output_tex,
			prim_lookup_buffer,
			scene_ubo_buffer,
			scene_desc_buffer,
		})
		.bind_texture_array(diffuse_textures)
		.bind(mesh_lights_buffer)
		.bind_tlas(instance->vkb.tlas);
	// Select a reservoir sample
	instance->vkb.rg
		->add_compute("Select Reservoir",
					  {.shader = Shader("src/shaders/integrators/vcm/select_reservoirs.comp"),
					   .dims = {(uint32_t)std::ceil(instance->width * instance->height / float(1024.0f)), 1, 1}})
		.bind(scene_desc_buffer)
		.push_constants(&pc_ray);

	// Update temporal reservoirs with the selected sample
	instance->vkb.rg
		->add_compute("Update Reservoirs",
					  {.shader = Shader("src/shaders/integrators/vcm/update_reservoirs.comp"),
					   .dims = {(uint32_t)std::ceil(instance->width * instance->height / float(1024.0f)), 1, 1}})
		.bind(scene_desc_buffer)
		.push_constants(&pc_ray);
	// Trace rays from eye
	instance->vkb.rg
		->add_rt("VCM - Trace Eye", {.shaders = {{"src/shaders/integrators/vcm/vcm_eye.rgen"},
												 {"src/shaders/ray.rmiss"},
												 {"src/shaders/ray_shadow.rmiss"},
												 {"src/shaders/ray.rchit"},
												 {"src/shaders/ray.rahit"}},
									 .dims = {instance->width, instance->height},
									 .accel = instance->vkb.tlas.accel})
		.push_constants(&pc_ray)
		.bind({
			output_tex,
			prim_lookup_buffer,
			scene_ubo_buffer,
			scene_desc_buffer,
		})
		.bind_texture_array(diffuse_textures)
		.bind(mesh_lights_buffer)
		.bind_tlas(instance->vkb.tlas);

	if (!do_spatiotemporal) {
		do_spatiotemporal = true;
	}
}

bool VCM::update() {
	pc_ray.frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		pc_ray.frame_num = 0;
	}
	return updated;
}
void VCM::destroy() {
	const auto device = instance->vkb.ctx.device;
	Integrator::destroy();
	std::vector<Buffer*> buffer_list = {&photon_buffer, &vcm_light_vertices_buffer, &light_path_cnt_buffer,
										&color_storage_buffer, &vcm_reservoir_buffer};
	for (auto b : buffer_list) {
		b->destroy();
	}
	std::vector<Pipeline*> pipeline_list = {vcm_light_pipeline.get(), vcm_eye_pipeline.get()};
	for (auto p : pipeline_list) {
		p->cleanup();
	}
	vkDestroyDescriptorSetLayout(device, desc_set_layout, nullptr);
	vkDestroyDescriptorPool(device, desc_pool, nullptr);
}
