#include "LumenPCH.h"
#include "SPPM.h"

void SPPM::init() {
	Integrator::init();

	sppm_data_buffer =
		prm::get_buffer({.name = "SPPM Data",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height()  * sizeof(SPPMData)});

	atomic_data_buffer =
		prm::get_buffer({.name = "Atomic Data",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = sizeof(AtomicData)});

	photon_buffer =
		prm::get_buffer({.name = "Photon Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = 10 * Window::width() * Window::height()  * sizeof(PhotonHash)});

	residual_buffer =
		prm::get_buffer({.name = "Residual Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height()  * 4 * sizeof(float)});

	counter_buffer =
		prm::get_buffer({.name = "Counter Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = sizeof(int)});

	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer->get_device_address();

	desc.material_addr = lumen_scene->materials_buffer->get_device_address();
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer->get_device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer->get_device_address();
	// SPPM
	desc.sppm_data_addr = sppm_data_buffer->get_device_address();
	desc.atomic_data_addr = atomic_data_buffer->get_device_address();
	desc.photon_addr = photon_buffer->get_device_address();
	desc.residual_addr = residual_buffer->get_device_address();
	desc.counter_addr = counter_buffer->get_device_address();
	lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = "Scene Desc",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	frame_num = 0;


	assert(vk::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, lumen_scene->prim_lookup_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, sppm_data_addr, sppm_data_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, atomic_data_addr, atomic_data_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, photon_addr, photon_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, residual_addr, residual_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, counter_addr, counter_buffer, vk::render_graph());
}

void SPPM::render() {
	pc_ray.size_x = Window::width();
	pc_ray.size_y = Window::height();
	pc_ray.num_lights = int(lumen_scene->gpu_lights.size());
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.total_light_area = lumen_scene->total_light_area;
	pc_ray.light_triangle_count = lumen_scene->total_light_triangle_cnt;
	pc_ray.frame_num = frame_num;
	// PPM related constants
	if (config->base_radius < 1e-7f) {
		config->base_radius = 1e-7f;
	}
	pc_ray.min_bounds = lumen_scene->m_dimensions.min;
	pc_ray.max_bounds = lumen_scene->m_dimensions.max;
	pc_ray.ppm_base_radius = config->base_radius;
	const glm::vec3 diam = pc_ray.max_bounds - pc_ray.min_bounds;
	const float max_comp = glm::max(diam.x, glm::max(diam.y, diam.z));
	const int base_grid_res = int(max_comp / config->base_radius);
	pc_ray.grid_res = glm::max(ivec3(diam * float(base_grid_res) / max_comp), ivec3(1));
	auto op_reduce = [&](const std::string& op_name, const std::string& op_shader_name, const std::string& reduce_name,
						 const std::string& reduce_shader_name) {
		uint32_t num_wgs = uint32_t((Window::width() * Window::height()  + 1023) / 1024);
		vk::render_graph()
			->add_compute(op_name, {.shader = vk::Shader(op_shader_name), .dims = {num_wgs, 1, 1}})
			.push_constants(&pc_ray)
			.bind(lumen_scene->scene_desc_buffer)
			.zero({residual_buffer, counter_buffer});
		while (num_wgs != 1) {
			vk::render_graph()
				->add_compute(reduce_name, {.shader = vk::Shader(reduce_shader_name), .dims = {num_wgs, 1, 1}})
				.push_constants(&pc_ray)
				.bind(lumen_scene->scene_desc_buffer);
			num_wgs = (uint32_t)(num_wgs + 1023) / 1024;
		}
	};

	const std::initializer_list<lumen::ResourceBinding> rt_bindings = {
		output_tex,
		scene_ubo_buffer,
		lumen_scene->scene_desc_buffer,
	};

	// Trace rays from eye
	vk::render_graph()
		->add_rt("SPPM - Eye",
				 {
					 .shaders = {{"src/shaders/integrators/sppm/sppm_eye.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .dims = {Window::width(), Window::height() },
				 })
		.push_constants(&pc_ray)
		.zero(photon_buffer)
		.zero(sppm_data_buffer, /*cond=*/pc_ray.frame_num == 0)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_as(tlas);
	// Calculate scene bbox given the calculated radius
	op_reduce("OpReduce: Max", "src/shaders/integrators/sppm/max.comp", "OpReduce: Reduce Max",
			  "src/shaders/integrators/sppm/reduce_max.comp");
	op_reduce("OpReduce: Min", "src/shaders/integrators/sppm/min.comp", "OpReduce: Reduce Min",
			  "src/shaders/integrators/sppm/reduce_min.comp");
	vk::render_graph()
		->add_compute("Bounds Calculation",
					  {.shader = vk::Shader("src/shaders/integrators/sppm/calc_bounds.comp"), .dims = {1, 1, 1}})
		.bind(lumen_scene->scene_desc_buffer);
	// Trace from light
	vk::render_graph()
		->add_rt("SPPM - Light",
				 {
					 .shaders = {{"src/shaders/integrators/sppm/sppm_light.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .dims = {Window::width(), Window::height() },
				 })
		.push_constants(&pc_ray)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_as(tlas);
	// Gather
	vk::render_graph()
		->add_compute("Gather",
					  {.shader = vk::Shader("src/shaders/integrators/sppm/gather.comp"),
					   .dims = {(uint32_t)std::ceil(Window::width() * Window::height()  / float(1024.0f)), 1, 1}})
		.push_constants(&pc_ray)
		.bind(lumen_scene->scene_desc_buffer)
		.bind_texture_array(lumen_scene->scene_textures);
	// Composite
	vk::render_graph()
		->add_compute("Composite",
					  {.shader = vk::Shader("src/shaders/integrators/sppm/composite.comp"),
					   .dims = {(uint32_t)std::ceil(Window::width() * Window::height()  / float(1024.0f)), 1, 1}})
		.push_constants(&pc_ray)
		.bind({output_tex, lumen_scene->scene_desc_buffer});
}

bool SPPM::update() {
	frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		frame_num = 0;
	}
	return updated;
}

void SPPM::destroy() {
	Integrator::destroy();
	auto buffer_list = {sppm_data_buffer, atomic_data_buffer, photon_buffer, residual_buffer,
						counter_buffer};
	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}

	if (desc_set_layout) vkDestroyDescriptorSetLayout(vk::context().device, desc_set_layout, nullptr);
	if (desc_pool) vkDestroyDescriptorPool(vk::context().device, desc_pool, nullptr);
}
