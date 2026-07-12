#include "Integrator.h"
#include "Path.h"

namespace path {

void init(Integrator* integrator) {
	Path& state = integrator->path;

	SceneDesc desc = integrator::scene_desc_base(integrator);
	integrator::upload_scene_desc(integrator, desc);

	integrator->frame_num = 0;

	assert(rg::settings().shader_inference == true);
	state.path_length = integrator->lumen_scene->config.common.path_length;
}

void render(Integrator* integrator) {
	Path& state = integrator->path;
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.num_lights = (i32)integrator->lumen_scene->gpu_lights.size;
	state.pc.time = lm::rand_u32();
	state.pc.max_depth = state.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc.dir_light_idx = integrator->lumen_scene->dir_light_idx;
	state.pc.frame_num = integrator->frame_num;
	state.pc.direct_lighting = state.direct_lighting;
	state.pc.enable_accumulation = state.enable_accumulation;
	rg::add_rt(CSTR("Path"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/path/path.rgen")},
								 {CSTR("src/shaders/ray.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/ray.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .dims = {Window::width(), Window::height()},
				 })
		.push_constants(&state.pc)
		.bind({
			integrator->output_tex,
			integrator->scene_ubo_buffer,
			integrator->lumen_scene->scene_desc_buffer,
		})
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		//.write(integrator->output_tex) // Needed if the automatic shader inference is disabled
		.bind_tlas(*integrator->tlas);
}

bool update(Integrator* integrator) {
	Path& state = integrator->path;
	integrator->frame_num++;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
	}
	return updated;
}

void destroy(Integrator* integrator, bool resize) {
	(void)integrator;
	(void)resize;
}

bool gui(Integrator* integrator) {
	Path& state = integrator->path;
	bool result = false;
	result |= ImGui::SliderInt("Path length", (i32*)&state.path_length, 0, 12);
	result |= ImGui::Checkbox("Direct lighting", &state.direct_lighting);
	result |= ImGui::Checkbox("Enable accumulation", &state.enable_accumulation);
	return result;
}

}  // namespace path
