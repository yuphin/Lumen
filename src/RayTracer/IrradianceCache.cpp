#include "LumenPCH.h"
#include "IrradianceCache.h"

void IrradianceCache::init() {
	Integrator::init();

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

	// At max, we spawn 1 surfel per tile.
	u32 tiles_x = (Window::width() + SURFELIZE_PASS_TILE_SIZE_XY - 1) / SURFELIZE_PASS_TILE_SIZE_XY;
	u32 tiles_y = (Window::height() + SURFELIZE_PASS_TILE_SIZE_XY - 1) / SURFELIZE_PASS_TILE_SIZE_XY;
	const u32 max_surfels_to_spawn = tiles_x * tiles_y;
	surfel_spawn_list_buffer =
		prm::get_buffer({.name = "Surfel Spawn List Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = max_surfels_to_spawn * sizeof(u32)});

	surfel_spawn_count_buffer =
		prm::get_buffer({.name = "Surfel Spawn Count Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(u32)});

	surfel_pool_buffer =
		prm::get_buffer({.name = "Surfel Pool",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = MAX_SURFEL_COUNT * sizeof(Surfel)});

	i32 free_stack_init_value = MAX_SURFEL_COUNT;
	surfel_free_stack_counter_buffer =
		prm::get_buffer({.name = "Surfel Free Stack Counter",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(i32),
						 .data = &free_stack_init_value});

	{
		lm::ScratchArena scratch = integrator_arena();
		lm::FixedArray<u32> free_stack = lm::fixed_array_create<u32>(scratch.arena, MAX_SURFEL_COUNT);

		for (u64 i = 0; i < MAX_SURFEL_COUNT; i++) {
			free_stack.push_back(MAX_SURFEL_COUNT - 1 - i);
		}

		surfel_free_stack_buffer =
			prm::get_buffer({.name = "Surfel Free Stack",
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 .memory_type = vk::BUFFER_TYPE_GPU,
							 .size = MAX_SURFEL_COUNT * sizeof(u32),
							 .data = free_stack.data});
	}

	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer->device_address();
	desc.material_addr = lumen_scene->materials_buffer->device_address();
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer->device_address();
	desc.compact_vertices_addr = lumen_scene->vertex_buffer->device_address();
	desc.g_buffer_addr = gbuffer->device_address();
	desc.transformations_addr = transformations_buffer->device_address();
	desc.surfel_spawn_list_addr = surfel_spawn_list_buffer->device_address();
	desc.surfel_spawn_count_addr = surfel_spawn_count_buffer->device_address();
	desc.surfel_pool_addr = surfel_pool_buffer->device_address();
	desc.surfel_free_stack_addr = surfel_free_stack_buffer->device_address();
	desc.surfel_free_stack_count_addr = surfel_free_stack_counter_buffer->device_address();

	lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = "Scene Desc",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	assert(vk::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, lumen_scene->prim_lookup_buffer, vk::render_graph());

	pc.desired_surfel_radius_px = 4;
	frame_num = 0;
}

void IrradianceCache::render() {
	pc.min_bounds = lumen_scene->dimensions.min;
	pc.max_bounds = lumen_scene->dimensions.max;
	pc.width = Window::width();
	pc.height = Window::height();
	pc.direct_lighting = direct_lighting;
	pc.frame_num = frame_num;
	pc.rand = rand();

	vk::render_graph()
		->add_rt(CSTR("GBuffer"),
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

	if (total_frame_idx == 0) {
		u32 max_tiles_x = (Window::width() + SURFELIZE_PASS_TILE_SIZE_XY - 1) / SURFELIZE_PASS_TILE_SIZE_XY;
		u32 max_tiles_y = (Window::height() + SURFELIZE_PASS_TILE_SIZE_XY - 1) / SURFELIZE_PASS_TILE_SIZE_XY;
		vk::render_graph()
			->add_compute(CSTR("Surfelize"),
						  {.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/surfelize.comp")),
						   .dims = {max_tiles_x, max_tiles_y, 1}})
			.push_constants(&pc)
			.zero(surfel_spawn_count_buffer)
			.bind({lumen_scene->scene_desc_buffer});

		vk::render_graph()
			->add_compute(
				CSTR("Allocate Surfels"),
				{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/surfel_allocate.comp")),
				 .dims = {(max_tiles_x * max_tiles_y + ALLOCATE_PASS_WG_SIZE - 1) / ALLOCATE_PASS_WG_SIZE, 1, 1}})
			.push_constants(&pc)
			.bind({lumen_scene->scene_desc_buffer, scene_ubo_buffer});
	}
	if (debug_mode) {
		vk::render_graph()
			->add_compute(CSTR("Debug"),
						  {.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/debug.comp")),
						   .dims = {(u32)std::ceil(Window::width() * Window::height() / f32(1024)), 1, 1}})
			.push_constants(&pc)
			.bind({lumen_scene->scene_desc_buffer, output_tex})
			.bind_texture_array(lumen_scene->scene_textures);
	}
}

bool IrradianceCache::update() {
	frame_num++;
	++total_frame_idx;
	bool updated = Integrator::update();
	if (updated) {
		frame_num = 0;
	}
	return updated;
}

bool IrradianceCache::gui() {
	bool result = Integrator::gui();
	result |= ImGui::Checkbox("Direct lighting", &direct_lighting);
	result |= ImGui::Checkbox("Debug mode", &debug_mode);
	result |= ImGui::SliderFloat("Surfel radius", &pc.desired_surfel_radius_px, 4, 128);
	return result;
}

void IrradianceCache::destroy(bool resize) {
	Integrator::destroy(resize);
	auto buffer_list = {gbuffer,
						transformations_buffer,
						surfel_spawn_list_buffer,
						surfel_spawn_count_buffer,
						surfel_pool_buffer,
						surfel_free_stack_counter_buffer,
						surfel_free_stack_buffer};
	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}
}
