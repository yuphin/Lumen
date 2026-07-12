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

	SceneDesc desc = integrator::scene_desc_base(integrator);
	// ReSTIR
	SET_SCENE_BUFFER(desc, g_buffer, state.g_buffer);
	SET_SCENE_BUFFER(desc, temporal_reservoir, state.temporal_reservoir_buffer);
	SET_SCENE_BUFFER(desc, spatial_reservoir, state.spatial_reservoir_buffer);
	SET_SCENE_BUFFER(desc, passthrough_reservoir, state.passthrough_reservoir_buffer);
	SET_SCENE_BUFFER(desc, color_storage, state.tmp_col_buffer);
	integrator::upload_scene_desc(integrator, desc);

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
	state.pc.enable_accumulation = state.enable_accumulation;
	state.pc.light_candidate_count = state.light_candidate_count;
	state.pc.enable_gi = state.enable_gi;
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
	result |= ImGui::Checkbox("Indirect lighting (GI)", &state.enable_gi);
	result |= ImGui::SliderInt("Light candidates", (i32*)&state.light_candidate_count, 1, 64);
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
