#include "LumenPCH.h"
#include "Path.h"

void Path::init() {
	Integrator::init();
	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer->get_device_address();

	desc.material_addr = lumen_scene->materials_buffer->get_device_address();
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer->get_device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer->get_device_address();
	lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = "Scene Desc",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	frame_num = 0;

	pc_ray.size_x = Window::width();
	pc_ray.size_y = Window::height();
	assert(vk::render_graph()->settings.shader_inference == true);
	// For shader resource dependency inference, use this macro to register a buffer address to the rendergraph
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, lumen_scene->prim_lookup_buffer,
								 vk::render_graph());
}

void Path::render() {
	vk::CommandBuffer cmd(/*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	pc_ray.num_lights = (int)lumen_scene->gpu_lights.size();
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.total_light_area = lumen_scene->total_light_area;
	pc_ray.light_triangle_count = lumen_scene->total_light_triangle_cnt;
	pc_ray.dir_light_idx = lumen_scene->dir_light_idx;
	pc_ray.frame_num = frame_num;
	vk::render_graph()
		->add_rt("Path",
				 {
					 .shaders = {{"src/shaders/integrators/path/path.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .dims = {Window::width(), Window::height() },
				 })
		.push_constants(&pc_ray)
		.bind({
			output_tex,
			scene_ubo_buffer,
			lumen_scene->scene_desc_buffer,
		})
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		//.write(output_tex) // Needed if the automatic shader inference is disabled
		.bind_tlas(tlas);
	vk::render_graph()->run_and_submit(cmd);
}

bool Path::update() {
	frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		frame_num = 0;
	}
	return updated;
}

void Path::destroy() { Integrator::destroy(); }
