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

	SceneDesc desc;
	desc.index_addr = integrator->lumen_scene->index_buffer->device_address();

	desc.material_addr = integrator->lumen_scene->materials_buffer->device_address();
	desc.prim_info_addr = integrator->lumen_scene->prim_lookup_buffer->device_address();
	desc.compact_vertices_addr = integrator->lumen_scene->vertex_buffer->device_address();
	// ReSTIR GI
	desc.restir_samples_addr = state.restir_samples_buffer->device_address();
	desc.restir_samples_old_addr = state.restir_samples_old_buffer->device_address();
	desc.temporal_reservoir_addr = state.temporal_reservoir_buffer->device_address();
	desc.spatial_reservoir_addr = state.spatial_reservoir_buffer->device_address();
	desc.color_storage_addr = state.tmp_col_buffer->device_address();
	integrator->lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = CSTR("Scene Desc"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	state.pc.total_light_area = 0;

	integrator->frame_num = 0;

	state.pc.total_frame_num = 0;
	state.pc.world_radius = integrator->lumen_scene->dimensions.radius;
	assert(rg::settings().shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, integrator->lumen_scene->prim_lookup_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, restir_samples_addr, state.restir_samples_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, restir_samples_old_addr, state.restir_samples_old_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, temporal_reservoir_addr, state.temporal_reservoir_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, spatial_reservoir_addr, state.spatial_reservoir_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, state.tmp_col_buffer);
}

void render(Integrator* integrator) {
	ReSTIRGI& state = integrator->restirgi;
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.num_lights = (i32)integrator->lumen_scene->gpu_lights.size;
	state.pc.random_num = rand() % U32_MAX;
	state.pc.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc.do_spatiotemporal = state.do_spatiotemporal;
	state.pc.total_light_area = integrator->lumen_scene->total_light_area;
	state.pc.total_light_count = integrator->lumen_scene->total_light_cnt;
	state.pc.enable_accumulation = state.enable_accumulation;
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
								 {CSTR("src/shaders/ray.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/ray.rchit")},
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

	// Spatial reuse
	rg::add_rt(CSTR("ReSTIRGI - Spatial Reuse"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/restir/gi/spatial_reuse.rgen")},
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
