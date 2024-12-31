#include "Framework/RenderGraph.h"
#include "LumenPCH.h"
#include "ReSTIRGI.h"

void ReSTIRGI::init() {
	Integrator::init();
	restir_samples_buffer = prm::get_buffer({
		.name = "ReSTIR Samples",
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.memory_type = vk::BufferType::GPU,
		.size = Window::width() * Window::height()  * sizeof(ReservoirSample),
	});

	restir_samples_old_buffer = prm::get_buffer({
		.name = "Old ReSTIR Samples",
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.memory_type = vk::BufferType::GPU,
		.size = Window::width() * Window::height()  * sizeof(ReservoirSample),
	});

	temporal_reservoir_buffer = prm::get_buffer({
		.name = "Temporal Reservoirs",
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.memory_type = vk::BufferType::GPU,
		.size = 2 * Window::width() * Window::height()  * sizeof(Reservoir),
	});

	spatial_reservoir_buffer = prm::get_buffer({
		.name = "Spatial Reservoirs",
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.memory_type = vk::BufferType::GPU,
		.size = 2 * Window::width() * Window::height()  * sizeof(Reservoir),
	});

	tmp_col_buffer = prm::get_buffer({
		.name = "Temp Color",
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.memory_type = vk::BufferType::GPU,
		.size = Window::width() * Window::height()  * sizeof(float) * 3,
	});

	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer->get_device_address();

	desc.material_addr = lumen_scene->materials_buffer->get_device_address();
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer->get_device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer->get_device_address();
	// ReSTIR GI
	desc.restir_samples_addr = restir_samples_buffer->get_device_address();
	desc.restir_samples_old_addr = restir_samples_old_buffer->get_device_address();
	desc.temporal_reservoir_addr = temporal_reservoir_buffer->get_device_address();
	desc.spatial_reservoir_addr = spatial_reservoir_buffer->get_device_address();
	desc.color_storage_addr = tmp_col_buffer->get_device_address();
	lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = "Scene Desc",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	pc_ray.total_light_area = 0;

	frame_num = 0;

	pc_ray.total_frame_num = 0;
	pc_ray.world_radius = lumen_scene->m_dimensions.radius;
	assert(vk::render_graph()->settings.shader_inference == true);
	lumen::RenderGraph* rg = vk::render_graph();
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, lumen_scene->prim_lookup_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, restir_samples_addr, restir_samples_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, restir_samples_old_addr, restir_samples_old_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, temporal_reservoir_addr, temporal_reservoir_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, spatial_reservoir_addr, spatial_reservoir_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, tmp_col_buffer, rg);
}

void ReSTIRGI::render() {
	pc_ray.size_x = Window::width();
	pc_ray.size_y = Window::height();
	pc_ray.num_lights = (int)lumen_scene->gpu_lights.size();
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.do_spatiotemporal = do_spatiotemporal;
	pc_ray.total_light_area = lumen_scene->total_light_area;
	pc_ray.light_triangle_count = lumen_scene->total_light_triangle_cnt;
	pc_ray.enable_accumulation = enable_accumulation;
	pc_ray.frame_num = frame_num;

	const std::initializer_list<lumen::ResourceBinding> rt_bindings = {
		output_tex,
		scene_ubo_buffer,
		lumen_scene->scene_desc_buffer,
	};

	// Trace rays
	vk::render_graph()
		->add_rt("ReSTIRGI - Generate Samples",
				 {
					 .shaders = {{"src/shaders/integrators/restir/gi/restir.rgen"},
								 {"src/shaders/ray.rmiss"},
								 {"src/shaders/ray_shadow.rmiss"},
								 {"src/shaders/ray.rchit"},
								 {"src/shaders/ray.rahit"}},
					 .dims = {Window::width(), Window::height() },
				 })
		.push_constants(&pc_ray)
		.zero(restir_samples_buffer)
		.zero(temporal_reservoir_buffer, !do_spatiotemporal)
		.zero(spatial_reservoir_buffer, !do_spatiotemporal)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_as(tlas)
		.copy(restir_samples_buffer, restir_samples_old_buffer);

	// Temporal reuse
	vk::render_graph()
		->add_rt("ReSTIRGI - Temporal Reuse",
				 {
					 .shaders = {{"src/shaders/integrators/restir/gi/temporal_reuse.rgen"},
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

	// Spatial reuse
	vk::render_graph()
		->add_rt("ReSTIRGI - Spatial Reuse",
				 {
					 .shaders = {{"src/shaders/integrators/restir/gi/spatial_reuse.rgen"},
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
	// Output
	vk::render_graph()
		->add_compute("Output",
					  {.shader = vk::Shader("src/shaders/integrators/restir/gi/output.comp"),
					   .dims = {(uint32_t)std::ceil(Window::width() * Window::height()  / float(1024.0f)), 1, 1}})
		.push_constants(&pc_ray)
		.bind({output_tex, lumen_scene->scene_desc_buffer});
	if (!do_spatiotemporal) {
		do_spatiotemporal = true;
	}
	pc_ray.total_frame_num++;
}

bool ReSTIRGI::update() {
	frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		frame_num = 0;
	}
	return updated;
}

bool ReSTIRGI::gui() {
	bool result = false;
	result |= ImGui::Checkbox("Enable accumulation", &enable_accumulation);
	return result;
}

void ReSTIRGI::destroy() {
	Integrator::destroy();
	auto buffer_list = {restir_samples_buffer, restir_samples_old_buffer, temporal_reservoir_buffer,
						spatial_reservoir_buffer, tmp_col_buffer};
	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}
}