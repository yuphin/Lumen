#include "LumenPCH.h"
#include "ReSTIRGI.h"

void ReSTIRGI::init() {
	Integrator::init();

	restir_samples_buffer.create("ReSTIR Samples", &instance->vkb.ctx,
								 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
								 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
								 instance->width * instance->height * sizeof(ReservoirSample));

	restir_samples_old_buffer.create("Old ReSTIR Samples", &instance->vkb.ctx,
									 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
										 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
									 instance->width * instance->height * sizeof(ReservoirSample));

	temporal_reservoir_buffer.create("Temporal Reservoirs", &instance->vkb.ctx,
									 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
										 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
									 2 * instance->width * instance->height * sizeof(Reservoir));

	spatial_reservoir_buffer.create("Spatial Reservoirs", &instance->vkb.ctx,
									VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
										VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
									2 * instance->width * instance->height * sizeof(Reservoir));

	tmp_col_buffer.create("Temp Color", &instance->vkb.ctx,
						  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
						  instance->width * instance->height * sizeof(float) * 3);

	SceneDesc desc;
	desc.vertex_addr = vertex_buffer.get_device_address();
	desc.index_addr = index_buffer.get_device_address();
	desc.normal_addr = normal_buffer.get_device_address();
	desc.uv_addr = uv_buffer.get_device_address();
	desc.material_addr = materials_buffer.get_device_address();
	desc.prim_info_addr = prim_lookup_buffer.get_device_address();
	// ReSTIR GI
	desc.restir_samples_addr = restir_samples_buffer.get_device_address();
	desc.restir_samples_old_addr = restir_samples_old_buffer.get_device_address();
	desc.temporal_reservoir_addr = temporal_reservoir_buffer.get_device_address();
	desc.spatial_reservoir_addr = spatial_reservoir_buffer.get_device_address();
	desc.color_storage_addr = tmp_col_buffer.get_device_address();
	scene_desc_buffer.create(
		&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneDesc), &desc, true);

	pc_ray.total_light_area = 0;
	pc_ray.frame_num = 0;
	pc_ray.total_frame_num = 0;
	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;
	pc_ray.world_radius = lumen_scene->m_dimensions.radius;
	assert(instance->vkb.rg->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, &prim_lookup_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, restir_samples_addr, &restir_samples_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, restir_samples_old_addr, &restir_samples_old_buffer,
								 instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, temporal_reservoir_addr, &temporal_reservoir_buffer,
								 instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, spatial_reservoir_addr, &spatial_reservoir_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, &tmp_col_buffer, instance->vkb.rg);
}

void ReSTIRGI::render() {
	CommandBuffer cmd(&instance->vkb.ctx, /*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	pc_ray.light_pos = scene_ubo.light_pos;
	pc_ray.light_type = 0;
	pc_ray.light_intensity = 10;
	pc_ray.num_lights = (int)lights.size();
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.do_spatiotemporal = do_spatiotemporal;
	pc_ray.total_light_area = total_light_area;
	pc_ray.light_triangle_count = total_light_triangle_cnt;
	
	const std::initializer_list<ResourceBinding> rt_bindings = {
		output_tex,
		scene_ubo_buffer,
		scene_desc_buffer,
	};

	// Trace rays
	instance->vkb.rg
		->add_rt("ReSTIRGI - Generate Samples", { .shaders = {{"src/shaders/integrators/restir/gi/restir.rgen"},
															 {"src/shaders/ray.rmiss"},
															 {"src/shaders/ray_shadow.rmiss"},
															 {"src/shaders/ray.rchit"},
															 {"src/shaders/ray.rahit"}},
												 .dims = {instance->width, instance->height},
												 .accel = instance->vkb.tlas.accel })
		.push_constants(&pc_ray)
		.zero(restir_samples_buffer)
		.zero(temporal_reservoir_buffer, !do_spatiotemporal)
		.zero(spatial_reservoir_buffer, !do_spatiotemporal)
		.bind(rt_bindings)
		.bind(mesh_lights_buffer)
		.bind_texture_array(scene_textures)
		.bind_tlas(instance->vkb.tlas)
		.copy(restir_samples_buffer, restir_samples_old_buffer);

	// Temporal reuse
	instance->vkb.rg
		->add_rt("ReSTIRGI - Temporal Reuse", {.shaders = {{"src/shaders/integrators/restir/gi/temporal_reuse.rgen"},
														   {"src/shaders/ray.rmiss"},
														   {"src/shaders/ray_shadow.rmiss"},
														   {"src/shaders/ray.rchit"},
														   {"src/shaders/ray.rahit"}},
											   .dims = {instance->width, instance->height},
											   .accel = instance->vkb.tlas.accel})
		.push_constants(&pc_ray)
		.bind(rt_bindings)
		.bind(mesh_lights_buffer)
		.bind_texture_array(scene_textures)
		.bind_tlas(instance->vkb.tlas);

	// Spatial reuse
	instance->vkb.rg
		->add_rt("ReSTIRGI - Spatial Reuse", {.shaders = {{"src/shaders/integrators/restir/gi/spatial_reuse.rgen"},
														  {"src/shaders/ray.rmiss"},
														  {"src/shaders/ray_shadow.rmiss"},
														  {"src/shaders/ray.rchit"},
														  {"src/shaders/ray.rahit"}},
											  .dims = {instance->width, instance->height},
											  .accel = instance->vkb.tlas.accel})
		.push_constants(&pc_ray)
		.bind(rt_bindings)
		.bind(mesh_lights_buffer)
		.bind_texture_array(scene_textures)
		.bind_tlas(instance->vkb.tlas);
	// Output
	instance->vkb.rg
		->add_compute("Output",
					  { .shader = Shader("src/shaders/integrators/restir/gi/output.comp"),
					   .dims = {(uint32_t)std::ceil(instance->width * instance->height / float(1024.0f)), 1, 1} })
		.push_constants(&pc_ray)
		.bind({ output_tex, scene_desc_buffer });
	if (!do_spatiotemporal) {
		do_spatiotemporal = true;
	}
	pc_ray.total_frame_num++;
	instance->vkb.rg->run_and_submit(cmd);
}

bool ReSTIRGI::update() {
	pc_ray.frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		pc_ray.frame_num = 0;
	}
	return updated;
}

void ReSTIRGI::destroy() {
	const auto device = instance->vkb.ctx.device;
	Integrator::destroy();
	std::vector<Buffer*> buffer_list = {&restir_samples_buffer, &restir_samples_old_buffer, &temporal_reservoir_buffer,
										&spatial_reservoir_buffer, &tmp_col_buffer};
	for (auto b : buffer_list) {
		b->destroy();
	}
}