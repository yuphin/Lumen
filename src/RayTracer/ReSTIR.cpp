#include "LumenPCH.h"
#include "ReSTIR.h"

void ReSTIR::init() {
	Integrator::init();

	g_buffer.create("G-Buffer",
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
						VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					instance->width * instance->height * sizeof(RestirGBufferData));

	temporal_reservoir_buffer.create("Temporal Reservoirs",
									 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
										 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
									 instance->width * instance->height * sizeof(RestirReservoir));

	passthrough_reservoir_buffer.create("Passthrough Buffer",
										VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
											VK_BUFFER_USAGE_TRANSFER_DST_BIT,
										VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
										instance->width * instance->height * sizeof(RestirReservoir));

	spatial_reservoir_buffer.create("Spatial Reservoirs",
									VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
										VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
									instance->width * instance->height * sizeof(RestirReservoir));

	tmp_col_buffer.create("Temporary Color",
						  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, instance->width * instance->height * sizeof(float) * 3);

	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer.get_device_address();

	desc.material_addr = lumen_scene->materials_buffer.get_device_address();
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer.get_device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer.get_device_address();
	// ReSTIR
	desc.g_buffer_addr = g_buffer.get_device_address();
	desc.temporal_reservoir_addr = temporal_reservoir_buffer.get_device_address();
	desc.spatial_reservoir_addr = spatial_reservoir_buffer.get_device_address();
	desc.passthrough_reservoir_addr = passthrough_reservoir_buffer.get_device_address();
	desc.color_storage_addr = tmp_col_buffer.get_device_address();
	lumen_scene->scene_desc_buffer.create(
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(SceneDesc), &desc, true);

	pc_ray.total_light_area = 0;

	frame_num = 0;

	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;
	assert(instance->vkb.rg->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, &lumen_scene->prim_lookup_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, g_buffer_addr, &g_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, temporal_reservoir_addr, &temporal_reservoir_buffer,
								 instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, spatial_reservoir_addr, &spatial_reservoir_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, passthrough_reservoir_addr, &passthrough_reservoir_buffer,
								 instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, &tmp_col_buffer, instance->vkb.rg);
}

void ReSTIR::render() {
	lumen::CommandBuffer cmd( /*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	pc_ray.num_lights = (int)lumen_scene->gpu_lights.size();
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.do_spatiotemporal = do_spatiotemporal;
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.total_light_area = lumen_scene->total_light_area;
	pc_ray.light_triangle_count = lumen_scene->total_light_triangle_cnt;
	pc_ray.enable_accumulation = enable_accumulation;
	pc_ray.frame_num = frame_num;

	const std::initializer_list<lumen::ResourceBinding> rt_bindings = {
		output_tex,
		scene_ubo_buffer,
		lumen_scene->scene_desc_buffer,
	};

	// Temporal pass + path tracing
	instance->vkb.rg
		->add_rt("ReSTIR - Temporal Pass",
				 {
					 .shaders = {{"src/shaders/integrators/restir/di/temporal_pass.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .dims = {instance->width, instance->height},
				 })
		.push_constants(&pc_ray)
		.zero(g_buffer)
		.zero(spatial_reservoir_buffer)
		.zero(temporal_reservoir_buffer, !do_spatiotemporal)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(instance->vkb.tlas);
	// Spatial pass
	instance->vkb.rg
		->add_rt("ReSTIR - Spatial Pass",
				 {
					 .shaders = {{"src/shaders/integrators/restir/di/spatial_pass.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .dims = {instance->width, instance->height},
				 })
		.push_constants(&pc_ray)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(instance->vkb.tlas);

	// Output
	instance->vkb.rg
		->add_rt("ReSTIR - Output",
				 {
					 .shaders = {{"src/shaders/integrators/restir/di/output.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .dims = {instance->width, instance->height},
				 })
		.push_constants(&pc_ray)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(instance->vkb.tlas);

	if (!do_spatiotemporal) {
		do_spatiotemporal = true;
	}
	instance->vkb.rg->run_and_submit(cmd);
}

bool ReSTIR::update() {
	frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		frame_num = 0;
	}
	return updated;
}

bool ReSTIR::gui() {
	bool result = false;
	result |= ImGui::Checkbox("Enable accumulation", &enable_accumulation);
	return result;
}

void ReSTIR::destroy() {
	Integrator::destroy();
	std::vector<lumen::Buffer*> buffer_list = {
		&g_buffer,		 &temporal_reservoir_buffer,	&spatial_reservoir_buffer,
		&tmp_col_buffer, &passthrough_reservoir_buffer,
	};
	for (auto b : buffer_list) {
		b->destroy();
	}
}
