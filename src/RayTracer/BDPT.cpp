#include "Integrator.h"
#include "BDPT.h"

void bdpt::init(Integrator* integrator) {
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

	assert(vk::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, integrator->lumen_scene->prim_lookup_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_path_addr, state.light_path_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, camera_path_addr, state.camera_path_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, state.color_storage_buffer, vk::render_graph());
}

void bdpt::render(Integrator* integrator) {
	BDPT& state = integrator->bdpt;
	state.pc_ray.num_lights = (i32)integrator->lumen_scene->gpu_lights.size;
	state.pc_ray.time = rand() % UINT_MAX;
	state.pc_ray.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc_ray.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc_ray.total_light_area = integrator->lumen_scene->total_light_area;
	state.pc_ray.light_triangle_count = integrator->lumen_scene->total_light_triangle_cnt;
	state.pc_ray.frame_num = integrator->frame_num;
	state.pc_ray.width = Window::width();
	state.pc_ray.height = Window::height();
	vk::render_graph()
		->add_rt(CSTR("BDPT"),
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
		.push_constants(&state.pc_ray)
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

bool bdpt::update(Integrator* integrator) {
	BDPT& state = integrator->bdpt;
	integrator->frame_num++;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
	}
	return updated;
}

void bdpt::destroy(Integrator* integrator, bool resize) {
	BDPT& state = integrator->bdpt;
	(void)resize;

	vk::Buffer** buffers[] = {&state.light_path_buffer, &state.camera_path_buffer, &state.color_storage_buffer};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}
}
