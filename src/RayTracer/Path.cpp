#include "LumenPCH.h"
#include "Path.h"

void Path::init() {
	Integrator::init();
	SceneDesc desc;
	desc.vertex_addr = vertex_buffer.get_device_address();
	desc.index_addr = index_buffer.get_device_address();
	desc.normal_addr = normal_buffer.get_device_address();
	desc.uv_addr = uv_buffer.get_device_address();
	desc.material_addr = materials_buffer.get_device_address();
	desc.prim_info_addr = prim_lookup_buffer.get_device_address();
	scene_desc_buffer.create(&instance->vkb.ctx,
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
							 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							 VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneDesc),
							 &desc, true);
	pc_ray.frame_num = 0;
	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;
}

void Path::render() {
	pc_ray.light_pos = scene_ubo.light_pos;
	pc_ray.light_type = 0;
	pc_ray.light_intensity = 10;
	pc_ray.num_lights = (int)lights.size();
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = lumen_scene->config.path_length;
	pc_ray.sky_col = lumen_scene->config.sky_col;
	pc_ray.total_light_area = total_light_area;
	pc_ray.light_triangle_count = total_light_triangle_cnt;

	instance->vkb.rg->
		add_rt("Path",
			   {
				   .pipeline_settings = {
					  .shaders = {
						  {"src/shaders/integrators/path/path.rgen"},
						  {"src/shaders/ray.rmiss"},
						  {"src/shaders/ray_shadow.rmiss"},
						  {"src/shaders/ray.rchit"},
						  {"src/shaders/ray.rahit"}
					  },
					  .push_consts_sizes = {sizeof(PushConstantRay)},
				   },
				   .dims = {instance->width, instance->height},
				   .accel = instance->vkb.tlas.accel
			   }
		)
		.push_constants(&pc_ray, sizeof(PushConstantRay))
		.write(output_tex)
		.bind({ 
				output_tex,
				prim_lookup_buffer,
				scene_ubo_buffer,
				scene_desc_buffer,  
			  })
		.bind_texture_array(diffuse_textures)
		.bind(mesh_lights_buffer)
		.bind_tlas(instance->vkb.tlas)
		.finalize();
}

bool Path::update() {
	bool updated = Integrator::update();
	if (updated) {
		pc_ray.frame_num = 0;
	}
	pc_ray.frame_num++;
	return updated;
}

void Path::destroy() {
	Integrator::destroy();
}
