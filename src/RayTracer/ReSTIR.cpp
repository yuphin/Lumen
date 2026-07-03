#include "Integrator.h"
#include "Framework/RenderGraph.h"
#include "ReSTIR.h"

namespace restir {

void init(Integrator* integrator) {
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
	SET_AND_REGISTER_BUFFER_ADDRESS(SceneDesc, desc, prim_info_addr, integrator->lumen_scene->prim_lookup_buffer);
	desc.compact_vertices_addr = integrator->lumen_scene->vertex_buffer->device_address();
	// ReSTIR
	SET_AND_REGISTER_BUFFER_ADDRESS(SceneDesc, desc, g_buffer_addr, state.g_buffer);
	SET_AND_REGISTER_BUFFER_ADDRESS(SceneDesc, desc, temporal_reservoir_addr, state.temporal_reservoir_buffer);
	SET_AND_REGISTER_BUFFER_ADDRESS(SceneDesc, desc, spatial_reservoir_addr, state.spatial_reservoir_buffer);
	SET_AND_REGISTER_BUFFER_ADDRESS(SceneDesc, desc, passthrough_reservoir_addr, state.passthrough_reservoir_buffer);
	SET_AND_REGISTER_BUFFER_ADDRESS(SceneDesc, desc, color_storage_addr, state.tmp_col_buffer);
	integrator->lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = CSTR("Scene Desc"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	state.pc.total_light_area = 0;

	integrator->frame_num = 0;
	assert(rg::settings().shader_inference == true);
}

void render(Integrator* integrator) {
	ReSTIR& state = integrator->restir;
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.num_lights = (i32)integrator->lumen_scene->gpu_lights.size;
	state.pc.time = lm::rand_u32();
	state.pc.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc.do_spatiotemporal = state.do_spatiotemporal;
	state.pc.random_num = lm::rand_u32();
	state.pc.total_light_area = integrator->lumen_scene->total_light_area;
	state.pc.total_light_count = integrator->lumen_scene->total_light_cnt;
	state.pc.enable_accumulation = state.enable_accumulation;
	state.pc.frame_num = integrator->frame_num;

	const std::initializer_list<lm::ResourceBinding> rt_bindings = {
		integrator->output_tex,
		integrator->scene_ubo_buffer,
		integrator->lumen_scene->scene_desc_buffer,
	};

	// Temporal pass + path tracing
	rg::add_rt(CSTR("ReSTIR - Temporal Pass"),
			   {
				   .shaders = {{CSTR("src/shaders/integrators/restir/di/temporal_pass.rgen")},
							   {CSTR("src/shaders/ray.rmiss")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rchit")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .dims = {Window::width(), Window::height() },
			   })
		.push_constants(&state.pc)
		.zero(state.g_buffer)
		.zero(state.spatial_reservoir_buffer)
		.zero(state.temporal_reservoir_buffer, !state.do_spatiotemporal)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	// Spatial pass
	rg::add_rt(CSTR("ReSTIR - Spatial Pass"),
			   {
				   .shaders = {{CSTR("src/shaders/integrators/restir/di/spatial_pass.rgen")},
							   {CSTR("src/shaders/ray.rmiss")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rchit")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .dims = {Window::width(), Window::height() },
			   })
		.push_constants(&state.pc)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);

	// Output
	rg::add_rt(CSTR("ReSTIR - Output"),
			   {
				   .shaders = {{CSTR("src/shaders/integrators/restir/di/output.rgen")},
							   {CSTR("src/shaders/ray.rmiss")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rchit")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .dims = {Window::width(), Window::height() },
			   })
		.push_constants(&state.pc)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);

	if (!state.do_spatiotemporal) {
		state.do_spatiotemporal = true;
	}
}

bool update(Integrator* integrator) {
	ReSTIR& state = integrator->restir;
	integrator->frame_num++;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
	}
	return updated;
}

bool gui(Integrator* integrator) {
	ReSTIR& state = integrator->restir;
	bool result = false;
	result |= ImGui::Checkbox("Enable accumulation", &state.enable_accumulation);
	return result;
}

void destroy(Integrator* integrator, bool resize) {
	ReSTIR& state = integrator->restir;
	(void)resize;

	vk::Buffer** buffers[] = {&state.g_buffer, &state.temporal_reservoir_buffer, &state.spatial_reservoir_buffer,
							 &state.tmp_col_buffer, &state.passthrough_reservoir_buffer};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}
}

}  // namespace restir
