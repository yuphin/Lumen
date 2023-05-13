#include "LumenPCH.h"
#include "ReSTIR.h"

void ReSTIR::init() {
	Integrator::init();

	g_buffer.create("G-Buffer", &instance->vkb.ctx,
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
						VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
					instance->width * instance->height * sizeof(GBufferData));

	temporal_reservoir_buffer.create("Temporal Reservoirs", &instance->vkb.ctx,
									 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
										 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
									 instance->width * instance->height * sizeof(RestirReservoir));

	passthrough_reservoir_buffer.create("Passthrough Buffer", &instance->vkb.ctx,
										VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
											VK_BUFFER_USAGE_TRANSFER_DST_BIT,
										VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
										instance->width * instance->height * sizeof(RestirReservoir));

	spatial_reservoir_buffer.create("Spatial Reservoirs", &instance->vkb.ctx,
									VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
										VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
									instance->width * instance->height * sizeof(RestirReservoir));

	tmp_col_buffer.create("Temporary Color", &instance->vkb.ctx,
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
	// ReSTIR
	desc.g_buffer_addr = g_buffer.get_device_address();
	desc.temporal_reservoir_addr = temporal_reservoir_buffer.get_device_address();
	desc.spatial_reservoir_addr = spatial_reservoir_buffer.get_device_address();
	desc.passthrough_reservoir_addr = passthrough_reservoir_buffer.get_device_address();
	desc.color_storage_addr = tmp_col_buffer.get_device_address();
	scene_desc_buffer.create(
		&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneDesc), &desc, true);

	pc_ray.total_light_area = 0;
	pc_ray.frame_num = 0;
	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;
	assert(instance->vkb.rg->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, &prim_lookup_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, g_buffer_addr, &g_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, temporal_reservoir_addr, &temporal_reservoir_buffer,
								 instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, spatial_reservoir_addr, &spatial_reservoir_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, passthrough_reservoir_addr, &passthrough_reservoir_buffer,
								 instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, &tmp_col_buffer, instance->vkb.rg);
}

void ReSTIR::render() {
	CommandBuffer cmd(&instance->vkb.ctx, /*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	pc_ray.num_lights = (int)lights.size();
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.do_spatiotemporal = do_spatiotemporal;
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.total_light_area = total_light_area;
	pc_ray.light_triangle_count = total_light_triangle_cnt;

	const std::initializer_list<ResourceBinding> rt_bindings = {
		output_tex,
		scene_ubo_buffer,
		scene_desc_buffer,
	};

	// Temporal pass + path tracing
	instance->vkb.rg
		->add_rt("ReSTIR - Temporal Pass", {.shaders = {{"src/shaders/integrators/restir/di/temporal_pass.rgen"},
														{"src/shaders/ray.rmiss"},
														{"src/shaders/ray_shadow.rmiss"},
														{"src/shaders/ray.rchit"},
														{"src/shaders/ray.rahit"}},
											.dims = {instance->width, instance->height},
											.accel = instance->vkb.tlas.accel})
		.push_constants(&pc_ray)
		.zero(g_buffer)
		.zero(spatial_reservoir_buffer)
		.zero(temporal_reservoir_buffer, !do_spatiotemporal)
		.bind(rt_bindings)
		.bind(mesh_lights_buffer)
		.bind_texture_array(scene_textures)
		.bind_tlas(instance->vkb.tlas);
	// Spatial pass
	instance->vkb.rg
		->add_rt("ReSTIR - Spatial Pass", {.shaders = {{"src/shaders/integrators/restir/di/spatial_pass.rgen"},
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
		->add_rt("ReSTIR - Output", {.shaders = {{"src/shaders/integrators/restir/di/output.rgen"},
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

	if (!do_spatiotemporal) {
		do_spatiotemporal = true;
	}
	instance->vkb.rg->run_and_submit(cmd);
}

bool ReSTIR::update() {
	pc_ray.frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		pc_ray.frame_num = 0;
	}
	return updated;
}

void ReSTIR::destroy() {
	const auto device = instance->vkb.ctx.device;
	Integrator::destroy();
	std::vector<Buffer*> buffer_list = {
		&g_buffer,		 &temporal_reservoir_buffer,	&spatial_reservoir_buffer,
		&tmp_col_buffer, &passthrough_reservoir_buffer,
	};
	for (auto b : buffer_list) {
		b->destroy();
	}
}
