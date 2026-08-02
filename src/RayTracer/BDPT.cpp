#include "Integrator.h"
#include "BDPT.h"

namespace bdpt {

void init(Integrator* integrator) {
	BDPT& state = integrator->bdpt;

	state.light_path_buffer =
		prm::get_buffer({.name = CSTR("Light Path Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * (integrator->lumen_scene->config.common.path_length + 1) *
								 sizeof(PathVertex)});
	state.camera_path_buffer =
		prm::get_buffer({.name = CSTR("Camera Path Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * (integrator->lumen_scene->config.common.path_length + 1) *
								 sizeof(PathVertex)});
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
	rg::add_rt(CSTR("BDPT"),
				 {

					 .shaders = {{CSTR("src/shaders/integrators/bdpt/bdpt.rgen")},
								 {CSTR("src/shaders/ray.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/ray.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .dims = {Window::width(), Window::height()},
				 })
		.zero(state.light_path_buffer)
		.zero(state.camera_path_buffer)
		.zero(state.color_storage_buffer)
		//.read(state.light_path_buffer) // Needed if shader inference is disabled
		//.read(state.camera_path_buffer)
		.push_constants(&state.pc)
		//.write(integrator->output_tex)
		.bind({
			integrator->output_tex,
			integrator->scene_ubo_buffer,
			integrator->lumen_scene->scene_desc_buffer,
		})
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
	return ImGui::Checkbox("Enable accumulation", &state.enable_accumulation);
}

void destroy(Integrator* integrator, bool resize) {
	BDPT& state = integrator->bdpt;
	(void)resize;

	vk::Buffer** buffers[] = {&state.light_path_buffer, &state.camera_path_buffer, &state.color_storage_buffer};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}
}

}  // namespace bdpt
