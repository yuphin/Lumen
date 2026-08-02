#include "Integrator.h"
#include "Framework/RenderGraph.h"
#include "ReSTIRGI.h"

namespace restirgi {

void init(Integrator* integrator) {
	ReSTIRGI& state = integrator->restirgi;

	state.restir_samples_buffer = prm::get_buffer({
		.name = CSTR("ReSTIR Samples"),
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.memory_type = vk::BUFFER_TYPE_GPU,
		.size = Window::width() * Window::height()  * sizeof(ReservoirSample),
	});

	state.restir_samples_old_buffer = prm::get_buffer({
		.name = CSTR("Old ReSTIR Samples"),
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.memory_type = vk::BUFFER_TYPE_GPU,
		.size = Window::width() * Window::height()  * sizeof(ReservoirSample),
	});

	state.temporal_reservoir_buffer = prm::get_buffer({
		.name = CSTR("Temporal Reservoirs"),
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.memory_type = vk::BUFFER_TYPE_GPU,
		.size = 2 * Window::width() * Window::height()  * sizeof(Reservoir),
	});

	state.spatial_reservoir_buffer = prm::get_buffer({
		.name = CSTR("Spatial Reservoirs"),
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.memory_type = vk::BUFFER_TYPE_GPU,
		.size = 2 * Window::width() * Window::height()  * sizeof(Reservoir),
	});

	state.tmp_col_buffer = prm::get_buffer({
		.name = CSTR("Temp Color"),
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.memory_type = vk::BUFFER_TYPE_GPU,
		.size = Window::width() * Window::height()  * sizeof(f32) * 3,
	});

	SceneDesc desc = integrator::scene_desc_base(integrator);
	// ReSTIR GI
	SET_SCENE_BUFFER(desc, restir_samples, state.restir_samples_buffer);
	SET_SCENE_BUFFER(desc, restir_samples_old, state.restir_samples_old_buffer);
	SET_SCENE_BUFFER(desc, temporal_reservoir, state.temporal_reservoir_buffer);
	SET_SCENE_BUFFER(desc, spatial_reservoir, state.spatial_reservoir_buffer);
	SET_SCENE_BUFFER(desc, color_storage, state.tmp_col_buffer);
	integrator::upload_scene_desc(integrator, desc);

	integrator->frame_num = 0;

	state.pc.total_frame_num = 0;
	state.pc.world_radius = integrator->lumen_scene->dimensions.radius;
	assert(rg::settings().shader_inference == true);
}

void render(Integrator* integrator) {
	ReSTIRGI& state = integrator->restirgi;
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.num_lights = (i32)integrator->lumen_scene->gpu_lights.size;
	state.pc.random_num = lm::rand_u32();
	state.pc.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc.do_spatiotemporal = state.do_spatiotemporal;
	state.pc.enable_accumulation = state.enable_accumulation;
	state.pc.enable_di = state.enable_di;
	state.pc.frame_num = integrator->frame_num;

	const std::initializer_list<lm::ResourceBinding> rt_bindings = {
		integrator->output_tex,
		integrator->scene_ubo_buffer,
		integrator->lumen_scene->scene_desc_buffer,
	};

	// Trace rays
	rg::add_rt(CSTR("ReSTIRGI - Generate Samples"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/restir/gi/restir.rgen")},
								 {CSTR("src/shaders/surface.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/surface.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .dims = {Window::width(), Window::height() },
				 })
		.push_constants(&state.pc)
		.zero(state.restir_samples_buffer)
		.zero(state.temporal_reservoir_buffer, !state.do_spatiotemporal)
		.zero(state.spatial_reservoir_buffer, !state.do_spatiotemporal)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas)
		.copy(state.restir_samples_buffer, state.restir_samples_old_buffer);

	// Temporal reuse
	rg::add_rt(CSTR("ReSTIRGI - Temporal Reuse"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/restir/gi/temporal_reuse.rgen")},
								 {CSTR("src/shaders/surface.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/surface.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .dims = {Window::width(), Window::height() },
				 })
		.push_constants(&state.pc)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);

	// Spatial reuse
	rg::add_rt(CSTR("ReSTIRGI - Spatial Reuse"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/restir/gi/spatial_reuse.rgen")},
								 {CSTR("src/shaders/surface.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/surface.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .dims = {Window::width(), Window::height() },
				 })
		.push_constants(&state.pc)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	// Output
	rg::add_compute(CSTR("Output"),
					  {.shader = vk::Shader(CSTR("src/shaders/integrators/restir/gi/output.comp")),
					   .dims = {(u32)lm::ceil(Window::width() * Window::height()  / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind({integrator->output_tex, integrator->lumen_scene->scene_desc_buffer});
	if (!state.do_spatiotemporal) {
		state.do_spatiotemporal = true;
	}
	state.pc.total_frame_num++;
}

bool update(Integrator* integrator) {
	ReSTIRGI& state = integrator->restirgi;
	integrator->frame_num++;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
	}
	return updated;
}

bool gui(Integrator* integrator) {
	ReSTIRGI& state = integrator->restirgi;
	bool result = false;
	result |= ImGui::Checkbox("Enable accumulation", &state.enable_accumulation);
	result |= ImGui::Checkbox("Direct lighting (DI)", &state.enable_di);
	return result;
}

void destroy(Integrator* integrator, bool resize) {
	ReSTIRGI& state = integrator->restirgi;
	(void)resize;

	vk::Buffer** buffers[] = {&state.restir_samples_buffer, &state.restir_samples_old_buffer,
							 &state.temporal_reservoir_buffer, &state.spatial_reservoir_buffer,
							 &state.tmp_col_buffer};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}
}

}  // namespace restirgi
