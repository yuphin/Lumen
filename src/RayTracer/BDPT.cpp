#include "Integrator.h"
#include "BDPT.h"

namespace bdpt {

void init(Integrator* integrator) {
	BDPT& state = integrator->bdpt;

	state.light_path_buffer =
		prm::get_buffer({.name = CSTR("Light Path Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() *
								 (integrator->lumen_scene->config.common.path_length + 1) * sizeof(PathVertex)});
	state.camera_path_buffer =
		prm::get_buffer({.name = CSTR("Camera Path Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() *
								 (integrator->lumen_scene->config.common.path_length + 1) * sizeof(PathVertex)});
	state.path_counts_buffer =
		prm::get_buffer({.name = CSTR("BDPT Path Counts"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(BDPTPathCounts)});
	state.color_storage_buffer =
		prm::get_buffer({.name = CSTR("Color Storage Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * 3 * 4});
	SceneDesc desc = integrator::scene_desc_base(integrator);
	// BDPT
	SET_SCENE_BUFFER(desc, light_path, state.light_path_buffer);
	SET_SCENE_BUFFER(desc, camera_path, state.camera_path_buffer);
	SET_SCENE_BUFFER(desc, path_cnt, state.path_counts_buffer);
	SET_SCENE_BUFFER(desc, color_storage, state.color_storage_buffer);
	integrator::upload_scene_desc(integrator, desc);

	integrator->frame_num = 0;

	assert(rg::settings().shader_inference == true);
}

void render(Integrator* integrator) {
	BDPT& state = integrator->bdpt;
	state.pc.num_lights = (i32)integrator->lumen_scene->gpu_lights.size;
	state.pc.time = lm::rand_u32();
	state.pc.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc.frame_num = integrator->frame_num;
	state.pc.enable_accumulation = state.enable_accumulation;
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.strategy_s = state.strategy_s;
	state.pc.strategy_t = state.strategy_t;
	state.pc.isolate_strategy = state.isolate_strategy;
	const std::initializer_list<lm::ResourceBinding> rt_bindings = {
		integrator->output_tex,
		integrator->scene_ubo_buffer,
		integrator->lumen_scene->scene_desc_buffer,
	};

	rg::add_rt(CSTR("BDPT - Trace Eye"),
			   {
				   .shaders = {{CSTR("src/shaders/integrators/bdpt/bdpt_eye.rgen")},
							   {CSTR("src/shaders/ray.rmiss")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rchit")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .dims = {Window::width(), Window::height()},
			   })
		.push_constants(&state.pc)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	rg::add_rt(CSTR("BDPT - Trace Light"),
			   {
				   .shaders = {{CSTR("src/shaders/integrators/bdpt/bdpt_light.rgen")},
							   {CSTR("src/shaders/ray.rmiss")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rchit")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .dims = {Window::width(), Window::height()},
			   })
		.push_constants(&state.pc)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	rg::add_rt(CSTR("BDPT - Connect"),
			   {
				   .shaders = {{CSTR("src/shaders/integrators/bdpt/bdpt_connect.rgen")},
							   {CSTR("src/shaders/ray.rmiss")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rchit")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .dims = {Window::width(), Window::height()},
			   })
		.zero(state.color_storage_buffer)
		.push_constants(&state.pc)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	rg::add_compute(CSTR("BDPT - Resolve Splats"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/bdpt/bdpt_resolve.comp")),
					 .dims = {(u32)lm::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind({integrator->output_tex, integrator->lumen_scene->scene_desc_buffer});
}

bool update(Integrator* integrator) {
	BDPT& state = integrator->bdpt;
	integrator->frame_num++;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
	}
	return updated;
}

bool gui(Integrator* integrator) {
	BDPT& state = integrator->bdpt;
	u32& path_length = integrator->lumen_scene->config.common.path_length;
	bool result = false;
	const bool path_length_changed = ImGui::SliderInt("Path length", (i32*)&path_length, 1, 12);
	result |= path_length_changed;
	result |= ImGui::Checkbox("Enable accumulation", &state.enable_accumulation);
	result |= ImGui::Checkbox("Isolate (s, t) strategy", &state.isolate_strategy);

	const i32 strategy_depth = state.strategy_s + state.strategy_t - 2;
	if (strategy_depth < 0 || strategy_depth >= (i32)path_length || (state.strategy_s == 1 && state.strategy_t == 1)) {
		state.strategy_s = 0;
		state.strategy_t = 2;
	}

	char selected_strategy[64];
	stbsp_snprintf(selected_strategy, ARRAY_LEN(selected_strategy), "s = %d, t = %d (depth %d)", state.strategy_s,
				   state.strategy_t, state.strategy_s + state.strategy_t - 2);
	ImGui::BeginDisabled(!state.isolate_strategy);
	if (ImGui::BeginCombo("Connection strategy", selected_strategy)) {
		for (i32 depth = 0; depth < (i32)path_length; depth++) {
			for (i32 s = 0; s <= depth + 1; s++) {
				const i32 t = depth + 2 - s;
				if (s == 1 && t == 1) {
					continue;
				}

				char strategy[64];
				stbsp_snprintf(strategy, ARRAY_LEN(strategy), "s = %d, t = %d (depth %d)", s, t, depth);
				const bool selected = state.strategy_s == s && state.strategy_t == t;
				if (ImGui::Selectable(strategy, selected)) {
					state.strategy_s = s;
					state.strategy_t = t;
					result = true;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::EndDisabled();
	ImGui::TextDisabled("s: light vertices, t: camera vertices");

	if (path_length_changed) {
		vkDeviceWaitIdle(vk::context().device);
		destroy(integrator, /*resize=*/false);
		init(integrator);
	}
	return result;
}

void destroy(Integrator* integrator, bool resize) {
	BDPT& state = integrator->bdpt;
	(void)resize;

	vk::Buffer** buffers[] = {&state.light_path_buffer, &state.camera_path_buffer, &state.path_counts_buffer,
							  &state.color_storage_buffer};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}
}

}  // namespace bdpt
