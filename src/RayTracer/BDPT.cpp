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
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * 3 * 4});
	SceneDesc desc;
	desc.index_addr = integrator->lumen_scene->index_buffer->device_address();

	desc.material_addr = integrator->lumen_scene->materials_buffer->device_address();
	desc.prim_info_addr = integrator->lumen_scene->prim_lookup_buffer->device_address();
	desc.compact_vertices_addr = integrator->lumen_scene->vertex_buffer->device_address();
	// BDPT
	desc.light_path_addr = state.light_path_buffer->device_address();
	desc.camera_path_addr = state.camera_path_buffer->device_address();
	desc.color_storage_addr = state.color_storage_buffer->device_address();

	integrator->lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = CSTR("Scene Desc"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	integrator->frame_num = 0;

	assert(rg::settings().shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, integrator->lumen_scene->prim_lookup_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_path_addr, state.light_path_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, camera_path_addr, state.camera_path_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, state.color_storage_buffer);
}

void render(Integrator* integrator) {
	BDPT& state = integrator->bdpt;
	state.pc.num_lights = (i32)integrator->lumen_scene->gpu_lights.size;
	state.pc.time = rand() % U32_MAX;
	state.pc.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc.total_light_area = integrator->lumen_scene->total_light_area;
	state.pc.total_light_count = integrator->lumen_scene->total_light_cnt;
	state.pc.frame_num = integrator->frame_num;
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
	//.finalize();
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
