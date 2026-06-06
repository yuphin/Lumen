#include "Integrator.h"
#include "Framework/RenderGraph.h"
#include "ReSTIR.h"

void restir::init(Integrator* integrator) {
	ReSTIR& state = integrator->restir;

	state.g_buffer = prm::get_buffer({.name = CSTR("G-Buffer"),
								.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
										 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								.memory_type = vk::BUFFER_TYPE_GPU,
								.size = Window::width() * Window::height()  * sizeof(RestirGBufferData)});

	state.temporal_reservoir_buffer =
		prm::get_buffer({.name = CSTR("Temporal Reservoirs"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height()  * sizeof(RestirReservoir)});

	state.passthrough_reservoir_buffer =
		prm::get_buffer({.name = CSTR("Passthrough Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height()  * sizeof(RestirReservoir)});

	state.spatial_reservoir_buffer =
		prm::get_buffer({.name = CSTR("Spatial Reservoirs"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height()  * sizeof(RestirReservoir)});

	state.tmp_col_buffer =
		prm::get_buffer({.name = CSTR("Temporary Color"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height()  * sizeof(f32) * 3});

	SceneDesc desc;
	desc.index_addr = integrator->lumen_scene->index_buffer->device_address();

	desc.material_addr = integrator->lumen_scene->materials_buffer->device_address();
	desc.prim_info_addr = integrator->lumen_scene->prim_lookup_buffer->device_address();
	desc.compact_vertices_addr = integrator->lumen_scene->vertex_buffer->device_address();
	// ReSTIR
	desc.g_buffer_addr = state.g_buffer->device_address();
	desc.temporal_reservoir_addr = state.temporal_reservoir_buffer->device_address();
	desc.spatial_reservoir_addr = state.spatial_reservoir_buffer->device_address();
	desc.passthrough_reservoir_addr = state.passthrough_reservoir_buffer->device_address();
	desc.color_storage_addr = state.tmp_col_buffer->device_address();
	integrator->lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = CSTR("Scene Desc"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	state.pc_ray.total_light_area = 0;

	integrator->frame_num = 0;


	lm::RenderGraph* rg = vk::render_graph();
	assert(rg->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, integrator->lumen_scene->prim_lookup_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, g_buffer_addr, state.g_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, temporal_reservoir_addr, state.temporal_reservoir_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, spatial_reservoir_addr, state.spatial_reservoir_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, passthrough_reservoir_addr, state.passthrough_reservoir_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, state.tmp_col_buffer, rg);
}

void restir::render(Integrator* integrator) {
	ReSTIR& state = integrator->restir;
	state.pc_ray.width = Window::width();
	state.pc_ray.height = Window::height();
	state.pc_ray.num_lights = (i32)integrator->lumen_scene->gpu_lights.size;
	state.pc_ray.time = rand() % UINT_MAX;
	state.pc_ray.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc_ray.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc_ray.do_spatiotemporal = state.do_spatiotemporal;
	state.pc_ray.random_num = rand() % UINT_MAX;
	state.pc_ray.total_light_area = integrator->lumen_scene->total_light_area;
	state.pc_ray.light_triangle_count = integrator->lumen_scene->total_light_triangle_cnt;
	state.pc_ray.enable_accumulation = state.enable_accumulation;
	state.pc_ray.frame_num = integrator->frame_num;

	const std::initializer_list<lm::ResourceBinding> rt_bindings = {
		integrator->output_tex,
		integrator->scene_ubo_buffer,
		integrator->lumen_scene->scene_desc_buffer,
	};

	lm::RenderGraph* rg = vk::render_graph();

	// Temporal pass + path tracing
	rg->add_rt(CSTR("ReSTIR - Temporal Pass"),
			   {
				   .shaders = {{CSTR("src/shaders/integrators/restir/di/temporal_pass.rgen")},
							   {CSTR("src/shaders/ray.rmiss")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rchit")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .dims = {Window::width(), Window::height() },
			   })
		.push_constants(&state.pc_ray)
		.zero(state.g_buffer)
		.zero(state.spatial_reservoir_buffer)
		.zero(state.temporal_reservoir_buffer, !state.do_spatiotemporal)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	// Spatial pass
	rg->add_rt(CSTR("ReSTIR - Spatial Pass"),
			   {
				   .shaders = {{CSTR("src/shaders/integrators/restir/di/spatial_pass.rgen")},
							   {CSTR("src/shaders/ray.rmiss")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rchit")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .dims = {Window::width(), Window::height() },
			   })
		.push_constants(&state.pc_ray)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);

	// Output
	rg->add_rt(CSTR("ReSTIR - Output"),
			   {
				   .shaders = {{CSTR("src/shaders/integrators/restir/di/output.rgen")},
							   {CSTR("src/shaders/ray.rmiss")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rchit")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .dims = {Window::width(), Window::height() },
			   })
		.push_constants(&state.pc_ray)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);

	if (!state.do_spatiotemporal) {
		state.do_spatiotemporal = true;
	}
}

bool restir::update(Integrator* integrator) {
	ReSTIR& state = integrator->restir;
	integrator->frame_num++;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
	}
	return updated;
}

bool restir::gui(Integrator* integrator) {
	ReSTIR& state = integrator->restir;
	bool result = false;
	result |= ImGui::Checkbox("Enable accumulation", &state.enable_accumulation);
	return result;
}

void restir::destroy(Integrator* integrator, bool resize) {
	ReSTIR& state = integrator->restir;
	(void)resize;

	vk::Buffer** buffers[] = {&state.g_buffer, &state.temporal_reservoir_buffer, &state.spatial_reservoir_buffer,
							 &state.tmp_col_buffer, &state.passthrough_reservoir_buffer};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}
}
