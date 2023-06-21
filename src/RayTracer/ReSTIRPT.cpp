#include "LumenPCH.h"
#include "ReSTIRPT.h"

void ReSTIRPT::init() {
	Integrator::init();

	std::vector<glm::mat4> transformations;
	transformations.resize(lumen_scene->prim_meshes.size());
	for (auto& pm : lumen_scene->prim_meshes) {
		transformations[pm.prim_idx] = pm.world_matrix;
	}

	gris_gbuffer.create("GRIS GBuffer", &instance->vkb.ctx,
						VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
						instance->width * instance->height * sizeof(GBuffer));

	direct_lighting_buffer.create("GRIS Direct Lighting", &instance->vkb.ctx,
								  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
								  instance->width * instance->height * sizeof(glm::vec3));

	gris_reservoir_buffer.create("GRIS Reservoirs", &instance->vkb.ctx,
								 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
								 instance->width * instance->height * sizeof(Reservoir));

	prefix_contribution_buffer.create("Prefix Contributions", &instance->vkb.ctx,
									  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
										  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
									  instance->width * instance->height * sizeof(glm::vec3));

	reconnection_buffer.create("Reservoir Connection", &instance->vkb.ctx,
							   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
							   instance->width * instance->height * sizeof(ReconnectionData));
	transformations_buffer.create("Transformations Buffer", &instance->vkb.ctx,
								  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
								  transformations.size() * sizeof(glm::mat4), transformations.data(), true);

	SceneDesc desc;
	desc.vertex_addr = vertex_buffer.get_device_address();
	desc.index_addr = index_buffer.get_device_address();
	desc.normal_addr = normal_buffer.get_device_address();
	desc.uv_addr = uv_buffer.get_device_address();
	desc.material_addr = materials_buffer.get_device_address();
	desc.prim_info_addr = prim_lookup_buffer.get_device_address();
	// ReSTIR PT (GRIS)
	desc.transformations_addr = transformations_buffer.get_device_address();
	desc.gris_gbuffer_addr = gris_gbuffer.get_device_address();
	desc.gris_reservoir_addr = gris_reservoir_buffer.get_device_address();
	desc.gris_direct_lighting_addr = direct_lighting_buffer.get_device_address();
	desc.prefix_contributions_addr = prefix_contribution_buffer.get_device_address();
	desc.reconnection_addr = reconnection_buffer.get_device_address();
	scene_desc_buffer.create(
		&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneDesc), &desc, true);

	pc_ray.total_light_area = 0;
	pc_ray.frame_num = 0;
	pc_ray.total_frame_num = 0;
	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;
	pc_ray.buffer_idx = 0;
	assert(instance->vkb.rg->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, &prim_lookup_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, gris_gbuffer_addr, &gris_gbuffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, gris_reservoir_addr, &gris_reservoir_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, gris_direct_lighting_addr, &direct_lighting_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, reconnection_addr, &reconnection_buffer, instance->vkb.rg);

	path_length = config->path_length;
}

void ReSTIRPT::render() {
	CommandBuffer cmd(&instance->vkb.ctx, /*start*/ true);
	pc_ray.num_lights = (int)lights.size();
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.max_depth = path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.total_light_area = total_light_area;
	pc_ray.light_triangle_count = total_light_triangle_cnt;
	pc_ray.dir_light_idx = lumen_scene->dir_light_idx;
	pc_ray.enable_accumulation = enable_accumulation;
	pc_ray.num_spatial_samples = num_spatial_samples;
	pc_ray.scene_extent = glm::length(lumen_scene->m_dimensions.max - lumen_scene->m_dimensions.min);
	pc_ray.direct_lighting = direct_lighting;
	pc_ray.enable_rr = enable_rr;

	pc_ray.spatial_radius = spatial_reuse_radius;
	pc_ray.enable_spatial_reuse = enable_spatial_reuse;
	pc_ray.show_reconnection_radiance = show_reconnection_radiance;
	pc_ray.min_vertex_distance_ratio = min_vertex_distance_ratio;

	const std::initializer_list<ResourceBinding> rt_bindings = {
		output_tex,
		scene_ubo_buffer,
		scene_desc_buffer,
	};

	const std::vector<ShaderMacro> macros = {{"MIS", enable_mis_in_gris}};
	// Trace rays
	instance->vkb.rg
		->add_rt("GRIS - Generate Samples", {.shaders = {{"src/shaders/integrators/restir/gris/gris.rgen"},
														 {"src/shaders/ray.rmiss"},
														 {"src/shaders/ray_shadow.rmiss"},
														 {"src/shaders/ray.rchit"},
														 {"src/shaders/ray.rahit"}},
											 .macros = macros,
											 .dims = {instance->width, instance->height},
											 .accel = instance->vkb.tlas.accel})
		.push_constants(&pc_ray)
		.zero(gris_reservoir_buffer)
		.bind(rt_bindings)
		.bind(mesh_lights_buffer)
		.bind_texture_array(scene_textures)
		.bind_tlas(instance->vkb.tlas);

	if (enable_experimental) {
		// Retrace
		instance->vkb.rg
			->add_rt("GRIS - Retrace Reservoirs",
					 {.shaders = {{"src/shaders/integrators/restir/gris/retrace_paths.rgen"},
								  {"src/shaders/ray.rmiss"},
								  {"src/shaders/ray_shadow.rmiss"},
								  {"src/shaders/ray.rchit"},
								  {"src/shaders/ray.rahit"}},
					  .macros = macros,
					  .dims = {instance->width, instance->height},
					  .accel = instance->vkb.tlas.accel})
			.push_constants(&pc_ray)
			.bind(rt_bindings)
			.bind(mesh_lights_buffer)
			.bind_texture_array(scene_textures)
			.bind_tlas(instance->vkb.tlas);

		//// Validate
		//instance->vkb.rg
		//	->add_rt("GRIS - Validate Samples",
		//			 {.shaders = {{"src/shaders/integrators/restir/gris/validate_samples.rgen"},
		//						  {"src/shaders/ray.rmiss"},
		//						  {"src/shaders/ray_shadow.rmiss"},
		//						  {"src/shaders/ray.rchit"},
		//						  {"src/shaders/ray.rahit"}},
		//			  .macros = macros,
		//			  .dims = {instance->width, instance->height},
		//			  .accel = instance->vkb.tlas.accel})
		//	.push_constants(&pc_ray)
		//	.bind(rt_bindings)
		//	.bind(mesh_lights_buffer)
		//	.bind_texture_array(scene_textures)
		//	.bind_tlas(instance->vkb.tlas);
	} else {
		// Spatial Reuse
		instance->vkb.rg
			->add_rt("GRIS - Spatial Reuse", {.shaders = {{"src/shaders/integrators/restir/gris/spatial_reuse.rgen"},
														  {"src/shaders/ray.rmiss"},
														  {"src/shaders/ray_shadow.rmiss"},
														  {"src/shaders/ray.rchit"},
														  {"src/shaders/ray.rahit"}},
											  .macros = macros,
											  .dims = {instance->width, instance->height},
											  .accel = instance->vkb.tlas.accel})
			.push_constants(&pc_ray)
			.bind(rt_bindings)
			.bind(mesh_lights_buffer)
			.bind_texture_array(scene_textures)
			.bind_tlas(instance->vkb.tlas);
	}

	pc_ray.total_frame_num++;
	instance->vkb.rg->run_and_submit(cmd);
}

bool ReSTIRPT::update() {
	pc_ray.frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		pc_ray.frame_num = 0;
	}
	return updated;
}

void ReSTIRPT::destroy() {
	const auto device = instance->vkb.ctx.device;
	Integrator::destroy();
	std::vector<Buffer*> buffer_list = {&gris_gbuffer, &gris_reservoir_buffer, &direct_lighting_buffer, &transformations_buffer};
	for (auto b : buffer_list) {
		b->destroy();
	}
}

bool ReSTIRPT::gui() {
	bool result = Integrator::gui();
	result |= ImGui::Checkbox("Enable accumulation", &enable_accumulation);
	result |= ImGui::Checkbox("Direct lighting", &direct_lighting);
	result |= ImGui::Checkbox("Enable Russian roulette", &enable_rr);
	result |= ImGui::Checkbox("Enable spatial reuse", &enable_spatial_reuse);
	result |= ImGui::Checkbox("Show reconnection radiance", &show_reconnection_radiance);
	result |= ImGui::Checkbox("Enable MIS in GRIS", &enable_mis_in_gris);
	result |= ImGui::Checkbox("Enable experimental", &enable_experimental);
	result |= ImGui::SliderInt("Num spatial samples", (int*)&num_spatial_samples, 0, 12);
	result |= ImGui::SliderInt("Path length", (int*)&path_length, 0, 12);
	result |= ImGui::SliderFloat("Spatial radius", &spatial_reuse_radius, 0.0f, 128.0f);
	result |= ImGui::SliderFloat("Min reconnection distance ratio", &min_vertex_distance_ratio, 0.0f, 1.0f);
	return result;
}
