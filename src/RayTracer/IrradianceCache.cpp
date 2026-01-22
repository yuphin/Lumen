#include "LumenPCH.h"
#include "IrradianceCache.h"

static constexpr uint HASH_TABLE_SIZE = 1 << 16;

void IrradianceCache::init() {
	Integrator::init();

	hash_cells_buffer = prm::get_buffer({
		.name = "Hash Cells",
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.memory_type = vk::BUFFER_TYPE_GPU,
		.size = HASH_TABLE_SIZE * sizeof(HashEntry),
	});
	gbuffer = prm::get_buffer({.name = "IRCache GBuffer",
							   .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
										VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							   .memory_type = vk::BUFFER_TYPE_GPU,
							   .size = Window::width() * Window::height() * sizeof(GBuffer)});

	std::vector<glm::mat4> transformations;
	transformations.resize(lumen_scene->prim_meshes.size);
	for (auto& pm : lumen_scene->prim_meshes) {
		transformations[pm.prim_idx] = pm.world_matrix;
	}
	transformations_buffer = prm::get_buffer({
		.name = "Transformations Buffer",
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		.memory_type = vk::BUFFER_TYPE_GPU,
		.size = transformations.size() * sizeof(glm::mat4),
		.data = transformations.data(),
	});

	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer->device_address();

	desc.material_addr = lumen_scene->materials_buffer->device_address();
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer->device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer->device_address();
	desc.g_buffer_addr = gbuffer->device_address();
	desc.transformations_addr = transformations_buffer->device_address();
	desc.hash_cells_addr = hash_cells_buffer->device_address();
	lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = "Scene Desc",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	frame_num = 0;

	pc.min_bounds = lumen_scene->dimensions.min;
	pc.max_bounds = lumen_scene->dimensions.max;
	pc.size_x = Window::width();
	pc.size_y = Window::height();
	pc.max_hash_table_size = HASH_TABLE_SIZE;
	assert(vk::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, lumen_scene->prim_lookup_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, hash_cells_addr, hash_cells_buffer, vk::render_graph());
}

void IrradianceCache::render() {
	pc.direct_lighting = direct_lighting;
	pc.min_cell_size = min_cell_size;
	pc.desired_px_per_cell = desired_px_per_cell;
	pc.frame_num = frame_num;
	pc.rand = rand();
	vk::render_graph()
		->add_compute(CSTR("Clear Hash Table"),
					  {.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/clear_cells.comp")),
					   .dims = {(HASH_TABLE_SIZE + 127) / 128}})
		.push_constants(&pc)
		.bind(hash_cells_buffer);
	vk::render_graph()
		->add_rt(CSTR("Irradiance Cache"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/irradiance_cache/primary.rgen")},
								 {CSTR("src/shaders/integrators/irradiance_cache/ray.rmiss")},
								 {CSTR("src/shaders/integrators/irradiance_cache/ray.rchit")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .dims = {Window::width(), Window::height()},
				 })
		.push_constants(&pc)
		.bind({output_tex, scene_ubo_buffer, lumen_scene->scene_desc_buffer, lumen_scene->mesh_lights_buffer})
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(tlas);

	if (1) {
		vk::render_graph()
			->add_rt(CSTR("Trace From IRCache Cells Debug"),
					 {
						 .shaders = {{CSTR("src/shaders/integrators/irradiance_cache/trace_from_cell_debug.rgen")},
									 {CSTR("src/shaders/integrators/irradiance_cache/ray.rmiss")},
									 {CSTR("src/shaders/integrators/irradiance_cache/ray.rchit")},
									 {CSTR("src/shaders/ray_shadow.rmiss")},
									 {CSTR("src/shaders/ray.rahit")}},
						 .dims = {Window::width(), Window::height()},
					 })
			.push_constants(&pc)
			.bind({output_tex, scene_ubo_buffer, lumen_scene->scene_desc_buffer, lumen_scene->mesh_lights_buffer})
			.bind_texture_array(lumen_scene->scene_textures)
			.bind_tlas(tlas);
	} else {
		vk::render_graph()
			->add_rt(CSTR("Trace From IRCache Cells"),
					 {
						 .shaders = {{CSTR("src/shaders/integrators/irradiance_cache/trace_from_cell.rgen")},
									 {CSTR("src/shaders/integrators/irradiance_cache/ray.rmiss")},
									 {CSTR("src/shaders/integrators/irradiance_cache/ray.rchit")},
									 {CSTR("src/shaders/ray_shadow.rmiss")},
									 {CSTR("src/shaders/ray.rahit")}},
						 .dims = {HASH_TABLE_SIZE},
					 })
			.push_constants(&pc)
			.bind({output_tex, scene_ubo_buffer, lumen_scene->scene_desc_buffer, lumen_scene->mesh_lights_buffer})
			.bind_texture_array(lumen_scene->scene_textures)
			.bind_tlas(tlas);
	}
}

bool IrradianceCache::update() {
	frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		frame_num = 0;
	}
	return updated;
}

bool IrradianceCache::gui() {
	bool result = Integrator::gui();
	result |= ImGui::Checkbox("Direct lighting", &direct_lighting);
	float max_cell_size = glm::distance(pc.min_bounds, pc.max_bounds) / 2.0f;
	result |= ImGui::SliderFloat("Min cell size", &min_cell_size, 0.1f, max_cell_size);
	result |= ImGui::SliderFloat("Desired px per cell", &desired_px_per_cell, 1.0f, 10.0f);
	return result;
}

void IrradianceCache::destroy(bool resize) {
	Integrator::destroy(resize);
	auto buffer_list = {gbuffer, transformations_buffer, hash_cells_buffer};
	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}
}
