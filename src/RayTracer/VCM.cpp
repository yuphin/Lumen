#include "LumenPCH.h"
#include "VCM.h"
#include <iostream>
#include <fstream>
const int max_samples = 50000;
static bool use_vc = true;
static bool written = false;
const bool ray_guide = false;
void VCM::init() {
	Integrator::init();
	photon_buffer.create("Photon Buffer",
						 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						 10 * instance->width * instance->height * sizeof(VCMPhotonHash));

	vcm_light_vertices_buffer.create(
		"VCM Light Vertices",
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		instance->width * instance->height * (config->path_length + 1) * sizeof(VCMVertex));

	light_path_cnt_buffer.create("Light Path Count",
								 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								 instance->width * instance->height * sizeof(float));

	color_storage_buffer.create("Color Storage",
								VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								instance->width * instance->height * 3 * sizeof(float));

	vcm_reservoir_buffer.create("VCM Reservoirs",
								VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								instance->width * instance->height * sizeof(VCMReservoir));

	light_samples_buffer.create("Light Samples",
								VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								instance->width * instance->height * sizeof(VCMRestirData));

	should_resample_buffer.create("Should Resample",
								  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 4);

	light_state_buffer.create("Light States",
							  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							  instance->width * instance->height * sizeof(LightState));

	angle_struct_buffer.create("Angle Struct",
							   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, max_samples * sizeof(AngleStruct));

	angle_struct_cpu_buffer.create(
								   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
									   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
								   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								   max_samples * sizeof(AngleStruct));

	avg_buffer.create("Average",
					  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
						  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
					  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(AvgStruct));

	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer.get_device_address();

	desc.material_addr = lumen_scene->materials_buffer.get_device_address();
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer.get_device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer.get_device_address();
	// VCM
	desc.photon_addr = photon_buffer.get_device_address();
	desc.vcm_vertices_addr = vcm_light_vertices_buffer.get_device_address();
	desc.path_cnt_addr = light_path_cnt_buffer.get_device_address();
	desc.color_storage_addr = color_storage_buffer.get_device_address();

	desc.vcm_reservoir_addr = vcm_reservoir_buffer.get_device_address();
	desc.light_samples_addr = light_samples_buffer.get_device_address();
	desc.should_resample_addr = should_resample_buffer.get_device_address();
	desc.light_state_addr = light_state_buffer.get_device_address();
	desc.angle_struct_addr = angle_struct_buffer.get_device_address();
	desc.avg_addr = avg_buffer.get_device_address();

	lumen_scene->scene_desc_buffer.create(
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(SceneDesc), &desc, true);
	pc_ray.total_light_area = 0;

	frame_num = 0;

	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;

	assert(lumen::VulkanBase::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, &lumen_scene->prim_lookup_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, photon_addr, &photon_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, vcm_vertices_addr, &vcm_light_vertices_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, path_cnt_addr, &light_path_cnt_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, &color_storage_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, vcm_reservoir_addr, &vcm_reservoir_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_samples_addr, &light_samples_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, should_resample_addr, &should_resample_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_state_addr, &light_state_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, angle_struct_addr, &angle_struct_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, avg_addr, &avg_buffer, lumen::VulkanBase::render_graph());
}

void VCM::render() {
	lumen::CommandBuffer cmd( /*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	const float ppm_base_radius = 0.25f;
	pc_ray.num_lights = int(lumen_scene->gpu_lights.size());
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.frame_num = frame_num;
	// VCM related constants
	pc_ray.radius = lumen_scene->m_dimensions.radius * config->radius_factor / 100.f;
	pc_ray.radius /= (float)pow((double)pc_ray.frame_num + 1, 0.5 * (1 - 2.0 / 3));
	pc_ray.min_bounds = lumen_scene->m_dimensions.min;
	pc_ray.max_bounds = lumen_scene->m_dimensions.max;
	pc_ray.use_vm = config->enable_vm;
	pc_ray.use_vc = use_vc;
	pc_ray.do_spatiotemporal = do_spatiotemporal;
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.max_angle_samples = max_samples;
	pc_ray.light_triangle_count = lumen_scene->total_light_triangle_cnt;
	const std::initializer_list<lumen::ResourceBinding> rt_bindings = {
		output_tex,
		scene_ubo_buffer,
		lumen_scene->scene_desc_buffer,
	};
	const glm::vec3 diam = pc_ray.max_bounds - pc_ray.min_bounds;
	const float max_comp = glm::max(diam.x, glm::max(diam.y, diam.z));
	const int base_grid_res = int(max_comp / pc_ray.radius);
	pc_ray.grid_res = glm::max(ivec3(diam * float(base_grid_res) / max_comp), ivec3(1));
	// Prepare
	auto& prepare_pass =
		lumen::VulkanBase::render_graph()
			->add_compute("Init Reservoirs",
						  {.shader = lumen::Shader("src/shaders/integrators/vcm/init_reservoirs.comp"),
						   .dims = {(uint32_t)std::ceil(instance->width * instance->height / float(1024.0f)), 1, 1}})
			.push_constants(&pc_ray)
			.bind(lumen_scene->scene_desc_buffer)
			.zero(photon_buffer, config->enable_vm);

	if (!do_spatiotemporal) {
		prepare_pass.zero({light_samples_buffer, should_resample_buffer});
	} else {
		prepare_pass.skip_execution();
	}

	// Do resampling
	lumen::VulkanBase::render_graph()
		->add_rt("Resample",
				 {
					 .shaders = {{"src/shaders/integrators/vcm/vcm_sample.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .dims = {instance->width, instance->height},
				 })
		.push_constants(&pc_ray)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(tlas);

	// Check resampling
	lumen::VulkanBase::render_graph()
		->add_compute("Check Reservoirs",
					  {.shader = lumen::Shader("src/shaders/integrators/vcm/check_reservoirs.comp"),
					   .dims = {(uint32_t)std::ceil(instance->width * instance->height / float(1024.0f)), 1, 1}})
		.push_constants(&pc_ray)
		.bind(lumen_scene->scene_desc_buffer)
		.zero(should_resample_buffer);
	pc_ray.random_num = rand() % UINT_MAX;
	// Spawn light rays
	lumen::VulkanBase::render_graph()
		->add_rt("VCM - Spawn Light",
				 {
					 .shaders = {{"src/shaders/integrators/vcm/vcm_spawn_light.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .dims = {instance->width, instance->height},
				 })
		.push_constants(&pc_ray)
		.zero(light_state_buffer)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(tlas);
	pc_ray.random_num = rand() % UINT_MAX;
	// Trace spawned rays
	lumen::VulkanBase::render_graph()
		->add_rt("VCM - Trace Light",
				 {
					 .shaders = {{"src/shaders/integrators/vcm/vcm_light.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .dims = {instance->width, instance->height},
				 })
		.push_constants(&pc_ray)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(tlas);
	// Select a reservoir sample
	lumen::VulkanBase::render_graph()
		->add_compute("Select Reservoir",
					  {.shader = lumen::Shader("src/shaders/integrators/vcm/select_reservoirs.comp"),
					   .dims = {(uint32_t)std::ceil(instance->width * instance->height / float(1024.0f)), 1, 1}})
		.bind(lumen_scene->scene_desc_buffer)
		.push_constants(&pc_ray);

	// Update temporal reservoirs with the selected sample
	lumen::VulkanBase::render_graph()
		->add_compute("Update Reservoirs",
					  {.shader = lumen::Shader("src/shaders/integrators/vcm/update_reservoirs.comp"),
					   .dims = {(uint32_t)std::ceil(instance->width * instance->height / float(1024.0f)), 1, 1}})
		.bind(lumen_scene->scene_desc_buffer)
		.push_constants(&pc_ray);
	// Trace rays from eye
	lumen::VulkanBase::render_graph()
		->add_rt("VCM - Trace Eye",
				 {
					 .shaders = {{"src/shaders/integrators/vcm/vcm_eye.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .dims = {instance->width, instance->height},
				 })
		.push_constants(&pc_ray)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(tlas);

	if (!do_spatiotemporal) {
		do_spatiotemporal = true;
	}
	pc_ray.total_frame_num++;
	lumen::VulkanBase::render_graph()->run_and_submit(cmd);
}

bool VCM::update() {
	frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		frame_num = 0;
	}
	return updated;
}
void VCM::destroy() {
	Integrator::destroy();
	std::vector<lumen::Buffer*> buffer_list = {&photon_buffer,
											   &vcm_light_vertices_buffer,
											   &light_path_cnt_buffer,
											   &color_storage_buffer,
											   &vcm_reservoir_buffer,
											   &light_samples_buffer,
											   &light_state_buffer,
											   &should_resample_buffer,
											   &angle_struct_buffer,
											   &angle_struct_cpu_buffer,
											   &avg_buffer};
	for (auto b : buffer_list) {
		b->destroy();
	}
	if (desc_set_layout) vkDestroyDescriptorSetLayout(vk::context().device, desc_set_layout, nullptr);
	if (desc_pool) vkDestroyDescriptorPool(vk::context().device, desc_pool, nullptr);
}
