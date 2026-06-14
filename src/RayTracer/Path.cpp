#include "Integrator.h"
#include "Path.h"

namespace path {

void init(Integrator* integrator) {
	Path& state = integrator->path;

	SceneDesc desc;
	desc.index_addr = integrator->lumen_scene->index_buffer->device_address();

	desc.material_addr = integrator->lumen_scene->materials_buffer->device_address();
	desc.prim_info_addr = integrator->lumen_scene->prim_lookup_buffer->device_address();
	desc.compact_vertices_addr = integrator->lumen_scene->vertex_buffer->device_address();
	integrator->lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = CSTR("Scene Desc"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	integrator->frame_num = 0;

	assert(rg::settings().shader_inference == true);
	// For shader resource dependency inference, use this macro to register a buffer address to the rendergraph
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, integrator->lumen_scene->prim_lookup_buffer);
	state.path_length = integrator->lumen_scene->config.common.path_length;
}

void render(Integrator* integrator) {
	Path& state = integrator->path;
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.num_lights = (i32)integrator->lumen_scene->gpu_lights.size;
	state.pc.time = rand() % U32_MAX;
	state.pc.max_depth = state.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc.total_light_area = integrator->lumen_scene->total_light_area;
	state.pc.total_light_count = integrator->lumen_scene->total_light_cnt;
	state.pc.dir_light_idx = integrator->lumen_scene->dir_light_idx;
	state.pc.frame_num = integrator->frame_num;
	state.pc.direct_lighting = state.direct_lighting;
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
	return result;
}

}  // namespace path
