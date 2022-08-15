#include "LumenPCH.h"
#include "SPPM.h"

void SPPM::init() {
	Integrator::init();
	sppm_data_buffer.create(
		"SPPM Data",
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		instance->width * instance->height * sizeof(SPPMData));

	atomic_data_buffer.create(
		"Atomic Data",
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE, sizeof(AtomicData));

	photon_buffer.create(
		"Photon Buffer",
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		10 * instance->width * instance->height * sizeof(PhotonHash));
	residual_buffer.create(
		"Residual Buffer",
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		instance->width * instance->height * 4 * sizeof(float));

	counter_buffer.create(
		"Counter Buffer",
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE, sizeof(int));

	SceneDesc desc;
	desc.vertex_addr = vertex_buffer.get_device_address();
	desc.index_addr = index_buffer.get_device_address();
	desc.normal_addr = normal_buffer.get_device_address();
	desc.uv_addr = uv_buffer.get_device_address();
	desc.material_addr = materials_buffer.get_device_address();
	desc.prim_info_addr = prim_lookup_buffer.get_device_address();
	// SPPM
	desc.sppm_data_addr = sppm_data_buffer.get_device_address();
	desc.atomic_data_addr = atomic_data_buffer.get_device_address();
	desc.photon_addr = photon_buffer.get_device_address();
	desc.residual_addr = residual_buffer.get_device_address();
	desc.counter_addr = counter_buffer.get_device_address();
	scene_desc_buffer.create(&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneDesc),
		&desc, true);
	pc_ray.frame_num = 0;
	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;


	instance->vkb.rg->settings.shader_inference = true;
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, sppm_data_addr, &sppm_data_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, atomic_data_addr, &atomic_data_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, photon_addr, &photon_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, residual_addr, &residual_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, counter_addr, &counter_buffer, instance->vkb.rg);
}

void SPPM::render() {
	CommandBuffer cmd(&instance->vkb.ctx, /*start*/ true,
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VkClearValue clear_color = { 0.25f, 0.25f, 0.25f, 1.0f };
	VkClearValue clear_depth = { 1.0f, 0 };
	VkViewport viewport = vk::viewport((float)instance->width,
		(float)instance->height, 0.0f, 1.0f);
	VkClearValue clear_values[] = { clear_color, clear_depth };
	pc_ray.light_pos = scene_ubo.light_pos;
	pc_ray.light_type = 0;
	pc_ray.light_intensity = 10;
	pc_ray.num_lights = int(lights.size());
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = lumen_scene->config.path_length;
	pc_ray.sky_col = lumen_scene->config.sky_col;
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.total_light_area = total_light_area;
	pc_ray.light_triangle_count = total_light_triangle_cnt;
	// PPM related constants
	pc_ray.radius = lumen_scene->config.base_radius;
	pc_ray.min_bounds = lumen_scene->m_dimensions.min;
	pc_ray.max_bounds = lumen_scene->m_dimensions.max;
	pc_ray.ppm_base_radius = lumen_scene->config.base_radius;
	const glm::vec3 diam = pc_ray.max_bounds - pc_ray.min_bounds;
	const float max_comp = glm::max(diam.x, glm::max(diam.y, diam.z));
	const int base_grid_res = int(max_comp / pc_ray.radius);
	pc_ray.grid_res =
		glm::max(ivec3(diam * float(base_grid_res) / max_comp), ivec3(1));
	if (pc_ray.radius < 1e-7f) {
		pc_ray.radius = 1e-7f;
	}
	auto op_reduce = [&](const std::string& op_name, const std::string& op_shader_name,
		const std::string& reduce_name, const std::string& reduce_shader_name) {
		uint32_t num_wgs = uint32_t((instance->width * instance->height + 1023) / 1024);
		instance->vkb.rg->add_compute(
			op_name,
			{
				.shader = Shader(op_shader_name),
				.dims = {num_wgs, 1, 1}
			}
		)
			.push_constants(&pc_ray)
			.bind(scene_desc_buffer)
			.zero({ residual_buffer,counter_buffer });
		while (num_wgs != 1) {
			instance->vkb.rg->add_compute(
				reduce_name,
				{
					.shader = Shader(reduce_shader_name),
					.dims = {num_wgs, 1, 1}
				}
			)
				.push_constants(&pc_ray)
				.bind(scene_desc_buffer);
			num_wgs = (uint32_t)(num_wgs + 1023) / 1024.0f;
		}
	};

	// Trace rays from eye
	instance->vkb.rg
		->add_rt(
		"SPPM - Eye",
		{
			.shaders = {{"src/shaders/integrators/sppm/sppm_eye.rgen"},
							 {"src/shaders/ray.rmiss"},
							 {"src/shaders/ray_shadow.rmiss"},
							 {"src/shaders/ray.rchit"},
							 {"src/shaders/ray.rahit"}},
			 .dims = {instance->width, instance->height},
			 .accel = instance->vkb.tlas.accel })
			 .push_constants(&pc_ray)
		.zero(photon_buffer)
		.zero(sppm_data_buffer, /*cond=*/pc_ray.frame_num == 0)
		.bind({
			output_tex,
			prim_lookup_buffer,
			scene_ubo_buffer,
			scene_desc_buffer,
		})
		.bind_texture_array(diffuse_textures)
		.bind(mesh_lights_buffer)
		.bind_tlas(instance->vkb.tlas);
	// Calculate scene bbox given the calculated radius
	op_reduce("OpReduce: Max", "src/shaders/integrators/sppm/max.comp",
		"OpReduce: Reduce Max", "src/shaders/integrators/sppm/reduce_max.comp");
	op_reduce("OpReduce: Min", "src/shaders/integrators/sppm/min.comp",
		"OpReduce: Reduce Min", "src/shaders/integrators/sppm/reduce_min.comp");
	instance->vkb.rg->add_compute(
		"Bounds Calculation",
		{
			.shader = Shader("src/shaders/integrators/sppm/calc_bounds.comp"),
			.dims = {1, 1, 1}
		}
	)
		.bind(scene_desc_buffer);
	// Trace from light
	instance->vkb.rg
		->add_rt(
		"SPPM - Light",
		{
			.shaders = {{"src/shaders/integrators/sppm/sppm_light.rgen"},
							 {"src/shaders/ray.rmiss"},
							 {"src/shaders/ray_shadow.rmiss"},
							 {"src/shaders/ray.rchit"},
							 {"src/shaders/ray.rahit"}},
			 .dims = {instance->width, instance->height},
			 .accel = instance->vkb.tlas.accel })
			 .push_constants(&pc_ray)
		.bind({
			output_tex,
			prim_lookup_buffer,
			scene_ubo_buffer,
			scene_desc_buffer,
		})
		.bind_texture_array(diffuse_textures)
		.bind(mesh_lights_buffer)
		.bind_tlas(instance->vkb.tlas);
	// Gather
	instance->vkb.rg->add_compute(
		"Gather",
		{
			.shader = Shader("src/shaders/integrators/sppm/gather.comp"),
			.dims = {(uint32_t)std::ceil(instance->width * instance->height / float(1024.0f)), 1, 1}
		}
	)
		.push_constants(&pc_ray)
		.bind(scene_desc_buffer)
		.bind_texture_array(diffuse_textures);
	// Composite
	instance->vkb.rg->add_compute(
		"Composite",
		{
			.shader = Shader("src/shaders/integrators/sppm/composite.comp"),
			.dims = {(uint32_t)std::ceil(instance->width * instance->height / float(1024.0f)), 1, 1}
		}
	)
		.push_constants(&pc_ray)
		.bind({ output_tex, scene_desc_buffer });
}

bool SPPM::update() {
	pc_ray.frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		pc_ray.frame_num = 0;
	}
	return updated;
}

void SPPM::destroy() {
	const auto device = instance->vkb.ctx.device;
	Integrator::destroy();
	std::vector<Buffer*> buffer_list = { &sppm_data_buffer, &atomic_data_buffer,
										 &photon_buffer,    &residual_buffer,
										 &counter_buffer,   &hash_buffer,
										 &tmp_col_buffer };
	for (auto b : buffer_list) {
		b->destroy();
	}

	vkDestroyDescriptorSetLayout(device, desc_set_layout, nullptr);
	vkDestroyDescriptorPool(device, desc_pool, nullptr);
}

