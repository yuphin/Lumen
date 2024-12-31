#include "Framework/RenderGraph.h"
#include "Framework/VulkanBase.h"
#include "LumenPCH.h"
#include "ReSTIR.h"

void ReSTIR::init() {
	Integrator::init();
	g_buffer = prm::get_buffer({.name = "G-Buffer",
								.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
										 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								.memory_type = vk::BufferType::GPU,
								.size = Window::width() * Window::height()  * sizeof(RestirGBufferData)});

	temporal_reservoir_buffer =
		prm::get_buffer({.name = "Temporal Reservoirs",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height()  * sizeof(RestirReservoir)});

	passthrough_reservoir_buffer =
		prm::get_buffer({.name = "Passthrough Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height()  * sizeof(RestirReservoir)});

	spatial_reservoir_buffer =
		prm::get_buffer({.name = "Spatial Reservoirs",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height()  * sizeof(RestirReservoir)});

	tmp_col_buffer =
		prm::get_buffer({.name = "Temporary Color",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = Window::width() * Window::height()  * sizeof(float) * 3});

	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer->get_device_address();

	desc.material_addr = lumen_scene->materials_buffer->get_device_address();
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer->get_device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer->get_device_address();
	// ReSTIR
	desc.g_buffer_addr = g_buffer->get_device_address();
	desc.temporal_reservoir_addr = temporal_reservoir_buffer->get_device_address();
	desc.spatial_reservoir_addr = spatial_reservoir_buffer->get_device_address();
	desc.passthrough_reservoir_addr = passthrough_reservoir_buffer->get_device_address();
	desc.color_storage_addr = tmp_col_buffer->get_device_address();
	lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = "Scene Desc",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BufferType::GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	pc_ray.total_light_area = 0;

	frame_num = 0;


	lumen::RenderGraph* rg = vk::render_graph();
	assert(rg->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, lumen_scene->prim_lookup_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, g_buffer_addr, g_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, temporal_reservoir_addr, temporal_reservoir_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, spatial_reservoir_addr, spatial_reservoir_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, passthrough_reservoir_addr, passthrough_reservoir_buffer, rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, tmp_col_buffer, rg);
}

void ReSTIR::render() {
	pc_ray.size_x = Window::width();
	pc_ray.size_y = Window::height();
	pc_ray.num_lights = (int)lumen_scene->gpu_lights.size();
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.do_spatiotemporal = do_spatiotemporal;
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.total_light_area = lumen_scene->total_light_area;
	pc_ray.light_triangle_count = lumen_scene->total_light_triangle_cnt;
	pc_ray.enable_accumulation = enable_accumulation;
	pc_ray.frame_num = frame_num;

	const std::initializer_list<lumen::ResourceBinding> rt_bindings = {
		output_tex,
		scene_ubo_buffer,
		lumen_scene->scene_desc_buffer,
	};

	lumen::RenderGraph* rg = vk::render_graph();

	// Temporal pass + path tracing
	rg->add_rt("ReSTIR - Temporal Pass",
			   {
				   .shaders = {{"src/shaders/integrators/restir/di/temporal_pass.rgen"},
							   {"src/shaders/ray.rmiss"},
							   {"src/shaders/ray_shadow.rmiss"},
							   {"src/shaders/ray.rchit"},
							   {"src/shaders/ray.rahit"}},
				   .dims = {Window::width(), Window::height() },
			   })
		.push_constants(&pc_ray)
		.zero(g_buffer)
		.zero(spatial_reservoir_buffer)
		.zero(temporal_reservoir_buffer, !do_spatiotemporal)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_as(tlas);
	// Spatial pass
	rg->add_rt("ReSTIR - Spatial Pass",
			   {
				   .shaders = {{"src/shaders/integrators/restir/di/spatial_pass.rgen"},
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
	rg->add_rt("ReSTIR - Output",
			   {
				   .shaders = {{"src/shaders/integrators/restir/di/output.rgen"},
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

	if (!do_spatiotemporal) {
		do_spatiotemporal = true;
	}
}

bool ReSTIR::update() {
	frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		frame_num = 0;
	}
	return updated;
}

bool ReSTIR::gui() {
	bool result = false;
	result |= ImGui::Checkbox("Enable accumulation", &enable_accumulation);
	return result;
}

void ReSTIR::destroy() {
	Integrator::destroy();
	auto buffer_list = {
		g_buffer, temporal_reservoir_buffer, spatial_reservoir_buffer, tmp_col_buffer, passthrough_reservoir_buffer,
	};
	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}
}
