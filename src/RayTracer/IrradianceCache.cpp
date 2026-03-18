#include "LumenPCH.h"
#include "IrradianceCache.h"

static u32 get_total_grid_cells() {
	u32 num_uniform_cells = GRID_CENTER_CELL_COUNT_AXIS * GRID_CENTER_CELL_COUNT_AXIS * GRID_CENTER_CELL_COUNT_AXIS;
	u32 num_trapezoidal_cells =
		6 * GRID_TRAPEZOIDAL_CELL_COUNT_AXIS * GRID_TRAPEZOIDAL_CELL_COUNT_AXIS * GRID_TRAPEZOIDAL_CELL_COUNT_AXIS;
	return num_uniform_cells + num_trapezoidal_cells;
}

static f32 get_max_uniform_cells(f32 desired_surfel_radius_px, f32 p11, f32 height) {
	f32 surfel_radius_factor = 2.0 * desired_surfel_radius_px / (p11 * height);
	return 1.0f / (2.0f * surfel_radius_factor);
}

static f32 get_px_size_per_trapezoidal_cell(f32 p11, f32 height) {
	f32 px_vertical = p11 * height / GRID_TRAPEZOIDAL_CELL_COUNT_AXIS;
	return fabsf(px_vertical);
}

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
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
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
			prm::get_buffer({.name = "Grid Cell Counts",
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 .memory_type = vk::BUFFER_TYPE_GPU,
							 .size = MAX_SURFEL_COUNT * sizeof(u32),
							 .data = free_stack.data});
	}

	u32 grid_total_cells = get_total_grid_cells();
	grid_cell_counts_buffer =
		prm::get_buffer({.name = "Grid Cell Counts",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = grid_total_cells * sizeof(u32)});

	grid_cell_offsets_buffer =
		prm::get_buffer({.name = "Grid Cell Offsets",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = (grid_total_cells + 1) * sizeof(i32)});

	grid_cell_stacks_buffer =
		prm::get_buffer({.name = "Grid Cell Stacks",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = grid_total_cells * sizeof(i32)});
	grid_cell_indices_buffer =
		prm::get_buffer({.name = "Grid Indices Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = GRID_AVG_SURFELS_PER_CELL * MAX_SURFEL_COUNT * sizeof(i32)});

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
	desc.grid_cell_counts_addr = grid_cell_counts_buffer->device_address();
	desc.grid_cell_offsets_addr = grid_cell_offsets_buffer->device_address();
	desc.grid_cell_stacks_addr = grid_cell_stacks_buffer->device_address();
	desc.grid_cell_indices_addr = grid_cell_indices_buffer->device_address();

	lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = "Scene Desc",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	assert(vk::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, lumen_scene->prim_lookup_buffer, vk::render_graph());

	pc.desired_surfel_radius_px = 8;
	pc.grid_uniform_cell_distance_threshold = 4.0f;
	frame_num = 0;
	// Clear surfel pool
	vk::CommandBuffer cmd(true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vkCmdFillBuffer(cmd.handle, surfel_pool_buffer->handle, 0, surfel_pool_buffer->size, 0);
	cmd.submit();

	f32 max_uniform_cells =
		get_max_uniform_cells(pc.desired_surfel_radius_px, scene_ubo.projection[1][1], Window::height());

	f32 max_trapezoidal_cell_size = get_px_size_per_trapezoidal_cell(scene_ubo.projection[1][1], Window::height());

	LUMEN_INFO("Uniform cells limit: %u", (u32)glm::round(fabsf(max_uniform_cells)));
	LUMEN_INFO("Trapezoidal cell size limit (px): %u", (u32)glm::round(max_trapezoidal_cell_size));
}

void IrradianceCache::render() {
	pc.min_bounds = lumen_scene->dimensions.min;
	pc.max_bounds = lumen_scene->dimensions.max;
	pc.width = Window::width();
	pc.height = Window::height();
	pc.direct_lighting = direct_lighting;
	pc.frame_num = frame_num;
	pc.rand = rand();
	u32 grid_total_cells = get_total_grid_cells();
	pc.grid_total_cells = grid_total_cells;
	pc.scene_extent = glm::length(lumen_scene->dimensions.max - lumen_scene->dimensions.min);

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
			->add_compute(CSTR("Surfel: Spawn"),
						  {.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/surfel_spawn.comp")),
						   .dims = {max_tiles_x, max_tiles_y, 1}})
			.push_constants(&pc)
			.zero(surfel_spawn_count_buffer)
			.bind({lumen_scene->scene_desc_buffer});

		vk::render_graph()
			->add_compute(
				CSTR("Surfel: Allocate"),
				{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/surfel_allocate.comp")),
				 .dims = {(max_tiles_x * max_tiles_y + ALLOCATE_PASS_WG_SIZE - 1) / ALLOCATE_PASS_WG_SIZE, 1, 1}})
			.push_constants(&pc)
			.bind({lumen_scene->scene_desc_buffer, scene_ubo_buffer});
	}

	////////////////////////////
	// --- Surfel Grid ---
	// Grid is rebuilt every frame
	vk::render_graph()
		->add_compute(CSTR("Grid: Clear"),
					  {.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/grid_clear.comp")),
					   .dims = {(grid_total_cells + ALLOCATE_PASS_WG_SIZE - 1) / ALLOCATE_PASS_WG_SIZE, 1, 1}})
		.push_constants(&pc)
		.bind({lumen_scene->scene_desc_buffer});
	// TODO: Having an indirect launch would be better here, maybe?
	vk::render_graph()
		->add_compute(CSTR("Grid: Count"),
					  {.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/grid_count.comp")),
					   .dims = {(MAX_SURFEL_COUNT + ALLOCATE_PASS_WG_SIZE - 1) / ALLOCATE_PASS_WG_SIZE, 1, 1}})
		.push_constants(&pc)
		.bind({lumen_scene->scene_desc_buffer, scene_ubo_buffer});

	if (debug_mode) {
		vk::render_graph()
			->add_compute(CSTR("Debug"),
						  {.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/debug.comp")),
						   .dims = {(u32)std::ceil(Window::width() * Window::height() / f32(1024)), 1, 1}})
			.push_constants(&pc)
			.bind({lumen_scene->scene_desc_buffer, output_tex, scene_ubo_buffer})
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
	result |= ImGui::SliderFloat("Uniform cell distance threshold", &pc.grid_uniform_cell_distance_threshold, 0.01, 10);
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
						surfel_free_stack_buffer,
						grid_cell_counts_buffer,
						grid_cell_offsets_buffer,
						grid_cell_stacks_buffer,
						grid_cell_indices_buffer};
	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}
}
