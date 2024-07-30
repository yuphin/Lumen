#include "../LumenPCH.h"
#include "BDPT.h"

void BDPT::init() {
	Integrator::init();
	light_path_buffer.create(
		
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		instance->width * instance->height * (lumen_scene->config->path_length + 1) * sizeof(PathVertex));

	camera_path_buffer.create(
		
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		instance->width * instance->height * (lumen_scene->config->path_length + 1) * sizeof(PathVertex));

	color_storage_buffer.create(
		 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, instance->width * instance->height * 3 * 4);
	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer.get_device_address();

	desc.material_addr = lumen_scene->materials_buffer.get_device_address();
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer.get_device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer.get_device_address();
	// BDPT
	desc.light_path_addr = light_path_buffer.get_device_address();
	desc.camera_path_addr = camera_path_buffer.get_device_address();
	desc.color_storage_addr = color_storage_buffer.get_device_address();

	lumen_scene->scene_desc_buffer.create(
		 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(SceneDesc), &desc, true);

	frame_num = 0;

	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;
	assert(lumen::VulkanBase::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, &lumen_scene->prim_lookup_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_path_addr, &light_path_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, camera_path_addr, &camera_path_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, &color_storage_buffer, lumen::VulkanBase::render_graph());
}

void BDPT::render() {
	lumen::CommandBuffer cmd( /*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	pc_ray.num_lights = (int)lumen_scene->gpu_lights.size();
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = lumen_scene->config->path_length;
	pc_ray.sky_col = lumen_scene->config->sky_col;
	pc_ray.total_light_area = lumen_scene->total_light_area;
	pc_ray.light_triangle_count = lumen_scene->total_light_triangle_cnt;
	pc_ray.frame_num = frame_num;
	lumen::VulkanBase::render_graph()
		->add_rt("BDPT",
				 {

					 .shaders = {{"src/shaders/integrators/bdpt/bdpt.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .dims = {instance->width, instance->height},
				 })
		.zero(light_path_buffer)
		.zero(camera_path_buffer)
		//.read(light_path_buffer) // Needed if shader inference is disabled
		//.read(camera_path_buffer)
		.push_constants(&pc_ray)
		//.write(output_tex)
		.bind({
			output_tex,
			scene_ubo_buffer,
			lumen_scene->scene_desc_buffer,
		})
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(tlas);
	//.finalize();

	lumen::VulkanBase::render_graph()->run_and_submit(cmd);
}

bool BDPT::update() {
	frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		frame_num = 0;
	}
	return updated;
}

void BDPT::destroy() {
	Integrator::destroy();
	std::vector<lumen::Buffer*> buffer_list = {&light_path_buffer, &camera_path_buffer, &color_storage_buffer};
	for (auto b : buffer_list) {
		b->destroy();
	}
}
