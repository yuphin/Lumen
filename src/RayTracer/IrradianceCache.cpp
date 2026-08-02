#include "Integrator.h"
#include "IrradianceCache.h"

namespace ircache {

using namespace IRCache;

////////////////////////////
// --- For debug purposes  ---
#define DEBUG_PASSES 1
static u32 _max_surfels_in_a_grid_cell = 0;
static float _avg_surfels_in_a_grid_cell = 0;
static bool _highlight_max_surfel_cell = false;
// --- //

static u32 get_total_grid_cells() {
	u32 num_uniform_cells = GRID_CENTER_CELL_COUNT_AXIS * GRID_CENTER_CELL_COUNT_AXIS * GRID_CENTER_CELL_COUNT_AXIS;
	u32 num_trapezoidal_cells =
		6 * GRID_TRAPEZOIDAL_CELL_COUNT_AXIS * GRID_TRAPEZOIDAL_CELL_COUNT_AXIS * GRID_TRAPEZOIDAL_CELL_COUNT_AXIS;
	// The +1 is for the last last cell when we're getting the surfel count from cell offsets
	return num_uniform_cells + num_trapezoidal_cells + 1;
}

static f32 get_max_uniform_cells(f32 desired_surfel_radius_px, f32 p11, f32 height) {
	f32 surfel_radius_factor = 2.0f * desired_surfel_radius_px / (p11 * height);
	return 1.0f / (2.0f * surfel_radius_factor);
}

static f32 get_px_size_per_trapezoidal_cell(f32 p11, f32 height) {
	f32 px_vertical = p11 * height / GRID_TRAPEZOIDAL_CELL_COUNT_AXIS;
	return fabsf(px_vertical);
}

static f32 get_max_surfel_radius_px(f32 p11, f32 height) {
	// Note: This ignores the tile period in surfel_spawn.comp
	// SURFELIZE_PASS_TILE_SIZE_XY == 16 means we process 16 x 16 pixels per tile
	// So for example a TILE_PERIOD of 3 implies we process 1 out of 9 tile in 1 frame
	// This gives us a max surfel diameter of 33 pixels without overlap  (r = 16.5)
	// If we we want to keep period lower, dedup pass needs to be enabled.
	return lm::floor(fabsf(p11 * height / (2.0f * lm::sqrt(3.0f) * GRID_TRAPEZOIDAL_CELL_COUNT_AXIS)));
}

static void scan(u32 num_wgs, vk::Buffer* scene_desc_buffer, const PCPrefixSum& pc, bool disable_sum_writes = false) {
	rg::add_compute(CSTR("PrefixScan - Scan"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/grid_scan.comp")),
					 .macros = {vk::ShaderMacro("DISABLE_SUM_WRITES", disable_sum_writes)},
					 .dims = {(u32)num_wgs, 1, 1}})
		.push_constants(&pc)
		.bind(scene_desc_buffer);
}
static void uniform_add(u32 num_wgs, vk::Buffer* scene_desc_buffer, const PCPrefixSum& pc) {
	rg::add_compute(CSTR("PrefixScan - Uniform Add"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/grid_uniform_add.comp")),
					 .dims = {(u32)num_wgs, 1, 1}})
		.push_constants(&pc)
		.bind(scene_desc_buffer);
}

static void prefix_scan(u32 num_elems, u32 block_sum_offset, u32 out_offset, u32 scratch_capacity, bool scan_sums,
						vk::Buffer* scene_desc_buffer) {
	u32 num_wgs = lm::max(1u, lm::div_ceil(num_elems, (u32)SCAN_WG_SIZE));
	PCPrefixSum pc = {
		.scan_sums = scan_sums,
		.num_elems = num_elems,
		.block_sum_offset = block_sum_offset,
		.out_offset = out_offset,
	};
	if (num_wgs > 1) {
		assert((u64)out_offset + num_wgs <= scratch_capacity);
		scan(num_wgs, scene_desc_buffer, pc);
		prefix_scan(num_wgs, out_offset, out_offset + num_wgs, scratch_capacity, /*scan_sums=*/true, scene_desc_buffer);
		uniform_add(num_wgs, scene_desc_buffer, pc);
	} else {
		scan(num_wgs, scene_desc_buffer, pc, /*disable_sum_writes=*/true);
	}
}

void init(Integrator* integrator) {
	IrradianceCache& state = integrator->ircache;
	PCIRCache& pc = state.pc;

	state.gbuffer =
		prm::get_buffer({.name = CSTR("IRCache GBuffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(SurfaceRef)});
	state.current_frame_lighting_buffer =
		prm::get_buffer({.name = CSTR("IRCache Current Frame Lighting"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(lm::vec3)});

	// At max, we spawn 1 surfel per tile.
	u32 tiles_x = (Window::width() + SURFELIZE_PASS_TILE_SIZE_XY - 1) / SURFELIZE_PASS_TILE_SIZE_XY;
	u32 tiles_y = (Window::height() + SURFELIZE_PASS_TILE_SIZE_XY - 1) / SURFELIZE_PASS_TILE_SIZE_XY;
	const u32 max_surfels_to_spawn = tiles_x * tiles_y;
	state.surfel_spawn_list_buffer =
		prm::get_buffer({.name = CSTR("Surfel Spawn List Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = max_surfels_to_spawn * sizeof(u32)});

	state.surfel_spawn_count_buffer =
		prm::get_buffer({.name = CSTR("Surfel Spawn Count Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(u32)});

	state.surfel_pool_buffer =
		prm::get_buffer({.name = CSTR("Surfel Pool"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = MAX_SURFEL_COUNT * sizeof(Surfel)});

	i32 free_stack_init_value = MAX_SURFEL_COUNT;
	state.surfel_free_stack_counter_buffer =
		prm::get_buffer({.name = CSTR("Surfel Free Stack Counter"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(i32),
						 .data = &free_stack_init_value});

	{
		lm::ScratchArena scratch = integrator->arena;
		lm::FixedArray<u32> free_stack = lm::fixed_array_create<u32>(scratch.arena, MAX_SURFEL_COUNT);

		for (u32 i = 0; i < MAX_SURFEL_COUNT; i++) {
			free_stack.push_back(MAX_SURFEL_COUNT - 1u - i);
		}

		state.surfel_free_stack_buffer =
			prm::get_buffer({.name = CSTR("Grid Free Stack"),
							 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 .memory_type = vk::BUFFER_TYPE_GPU,
							 .size = MAX_SURFEL_COUNT * sizeof(u32),
							 .data = free_stack.data});
	}

	u32 grid_total_cells = get_total_grid_cells();
	state.grid_cell_counts_buffer =
		prm::get_buffer({.name = CSTR("Grid Cell Counts"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = DEBUG_PASSES ? vk::BUFFER_TYPE_GPU_TO_CPU : vk::BUFFER_TYPE_GPU,
						 .size = grid_total_cells * sizeof(u32)});

	state.grid_cell_indices_buffer =
		prm::get_buffer({.name = CSTR("Grid Indices Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = GRID_AVG_SURFELS_PER_CELL * MAX_SURFEL_COUNT * sizeof(i32)});

	u64 prefix_sum_scratch_elements = 0;
	u64 cur_total_cells = grid_total_cells;
	do {
		u64 num_blocks = lm::max(1uLL, lm::div_ceil(cur_total_cells, (u64)SCAN_WG_SIZE));
		if (num_blocks > 1) {
			prefix_sum_scratch_elements += num_blocks;
		}
		cur_total_cells = num_blocks;
	} while (cur_total_cells > 1);
	assert(prefix_sum_scratch_elements <= U32_MAX);

	state.grid_prefix_sum_scratch_buffer =
		prm::get_buffer({.name = CSTR("Grid Prefix Sum Scratch"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = VkDeviceSize(prefix_sum_scratch_elements * sizeof(u32))});

	state.surfel_samples_buffer =
		prm::get_buffer({.name = CSTR("Surfel Samples"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = state.rays_per_surfel * MAX_SURFEL_COUNT * sizeof(IRCache::SurfelSample)});

	SceneDesc desc = integrator::scene_desc_base(integrator);
	// IRCache
	SET_SCENE_BUFFER(desc, g_buffer, state.gbuffer);
	SET_SCENE_BUFFER(desc, direct_lighting, state.current_frame_lighting_buffer);
	SET_SCENE_BUFFER(desc, surfel_spawn_list, state.surfel_spawn_list_buffer);
	SET_SCENE_BUFFER(desc, surfel_spawn_count, state.surfel_spawn_count_buffer);
	SET_SCENE_BUFFER(desc, surfel_pool, state.surfel_pool_buffer);
	SET_SCENE_BUFFER(desc, surfel_free_stack, state.surfel_free_stack_buffer);
	SET_SCENE_BUFFER(desc, surfel_free_stack_count, state.surfel_free_stack_counter_buffer);
	SET_SCENE_BUFFER(desc, grid_cell_counts, state.grid_cell_counts_buffer);
	SET_SCENE_BUFFER(desc, grid_cell_indices, state.grid_cell_indices_buffer);
	SET_SCENE_BUFFER(desc, grid_prefix_sum_scratch, state.grid_prefix_sum_scratch_buffer);
	SET_SCENE_BUFFER(desc, surfel_samples, state.surfel_samples_buffer);

	integrator::upload_scene_desc(integrator, desc);

	assert(rg::settings().shader_inference == true);

	integrator->frame_num = 0;
	// Clear surfel pool
	vk::CommandBuffer cmd(true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vkCmdFillBuffer(cmd.handle, state.surfel_pool_buffer->handle, 0, state.surfel_pool_buffer->size, 0);
	cmd.submit();

	f32 max_uniform_cells = get_max_uniform_cells(pc.desired_surfel_radius_px, integrator->scene_ubo.projection[1][1],
												  (f32)Window::height());

	pc.desired_surfel_radius_px =
		get_max_surfel_radius_px(integrator->scene_ubo.projection[1][1], (f32)Window::height());
	pc.grid_uniform_cell_distance_threshold = 0.1f;

	f32 max_trapezoidal_cell_size =
		get_px_size_per_trapezoidal_cell(integrator->scene_ubo.projection[1][1], (f32)Window::height());
	LUMEN_INFO("Uniform cells size limit (world space): %u", (u32)lm::round(fabsf(max_uniform_cells)));
	LUMEN_INFO("Trapezoidal cell size limit (px): %u", (u32)lm::round(max_trapezoidal_cell_size));
}

void render(Integrator* integrator) {
	IrradianceCache& state = integrator->ircache;
	PCIRCache& pc = state.pc;
	pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	pc.frame_num = integrator->frame_num;
	pc.min_bounds = integrator->lumen_scene->dimensions.min;
	pc.width = Window::width();
	pc.max_bounds = integrator->lumen_scene->dimensions.max;
	pc.height = Window::height();
	pc.num_lights = (i32)integrator->lumen_scene->gpu_lights.size;
	pc.dir_light_idx = integrator->lumen_scene->dir_light_idx;
	pc.direct_lighting = state.direct_lighting;
	pc.enable_accumulation = state.enable_accumulation;
	pc.sampling_seed = lm::rand_u32();
	u32 grid_total_cells = get_total_grid_cells();
	u32 prefix_sum_scratch_capacity = (u32)(state.grid_prefix_sum_scratch_buffer->size / sizeof(u32));
	pc.grid_total_cells = grid_total_cells;
	pc.scene_extent = lm::length(integrator->lumen_scene->dimensions.max - integrator->lumen_scene->dimensions.min);
	pc.total_frame_num = state.total_frame_idx;
	pc.rays_per_surfel = state.rays_per_surfel;
	pc.use_camera_relative_surfel_size = state.use_camera_relative_surfel_size;
	pc.debug_view = state.debug_view;

	vk::CommandBuffer cmd;
	if (DEBUG_PASSES) {
		cmd.begin();
	}

	rg::add_rt(CSTR("GBuffer"),
			   {
				   .shaders = {{CSTR("src/shaders/integrators/irradiance_cache/primary.rgen")},
								   {CSTR("src/shaders/surface.rmiss")},
								   {CSTR("src/shaders/surface.rchit")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .dims = {Window::width(), Window::height()},
			   })
		.push_constants(&pc)
		.bind({integrator->output_tex, integrator->scene_ubo_buffer, integrator->lumen_scene->scene_desc_buffer,
			   integrator->lumen_scene->mesh_lights_buffer})
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);

	rg::add_compute(CSTR("Surfel: Recycle"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/surfel_recycle.comp")),
					 .dims = {lm::div_ceil(MAX_SURFEL_COUNT, ALLOCATE_PASS_WG_SIZE), 1, 1}})
		.push_constants(&pc)
		.bind({integrator->lumen_scene->scene_desc_buffer, integrator->scene_ubo_buffer});

	////////////////////////////
	// --- Surfel Grid ---
	// Grid is rebuilt every frame
	rg::add_compute(CSTR("Grid: Clear"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/grid_clear.comp")),
					 .dims = {lm::div_ceil(grid_total_cells, (u32)ALLOCATE_PASS_WG_SIZE), 1, 1}})
		.push_constants(&pc)
		.bind({integrator->lumen_scene->scene_desc_buffer});
	// TODO: Having an indirect launch would be better here, maybe?
	rg::add_compute(CSTR("Grid: Count"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/grid_count.comp")),
					 .dims = {lm::div_ceil((u32)MAX_SURFEL_COUNT, (u32)ALLOCATE_PASS_WG_SIZE), 1, 1}})
		.push_constants(&pc)
		.bind({integrator->lumen_scene->scene_desc_buffer, integrator->scene_ubo_buffer});

	if (DEBUG_PASSES) {
		// Debug
		rg::run_and_submit(cmd);
		u32* counts = (u32*)vk::buffer_map(state.grid_cell_counts_buffer);
		lm::ScratchArena scratch = integrator->arena;

		u64 total_cells = get_total_grid_cells();
		auto prefix_sums = lm::fixed_array_create<u32>(scratch.arena, total_cells);


		for (u64 i = 0; i < total_cells; i++) {
			u32 prev = i > 0 ? prefix_sums[i - 1] : 0;
			prefix_sums.push_back(prev + counts[i]);
		}
		vk::buffer_unmap(state.grid_cell_counts_buffer);

		cmd.begin();
		prefix_scan(grid_total_cells, 0, 0, prefix_sum_scratch_capacity, /*scan_sums=*/false,
					integrator->lumen_scene->scene_desc_buffer);
		rg::run_and_submit(cmd);

		u32* gpu_prefix_sums = (u32*)vk::buffer_map(state.grid_cell_counts_buffer);
		for (u64 i = 0; i < total_cells; i++) {
			assert(gpu_prefix_sums[i] == prefix_sums[i]);
		}
		vk::buffer_unmap(state.grid_cell_counts_buffer);
		cmd.begin();

	} else {
		prefix_scan(grid_total_cells, 0, 0, prefix_sum_scratch_capacity, /*scan_sums=*/false,
					integrator->lumen_scene->scene_desc_buffer);
	}

	rg::add_compute(CSTR("Grid: Distribute"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/grid_distribute.comp")),
					 .macros = {vk::ShaderMacro("DEBUG_GRID_INVARIANTS", DEBUG_PASSES)},
					 .dims = {lm::div_ceil((u32)MAX_SURFEL_COUNT, (u32)ALLOCATE_PASS_WG_SIZE), 1, 1}})
		.push_constants(&pc)
		.bind({integrator->lumen_scene->scene_desc_buffer, integrator->scene_ubo_buffer});

	if (state.enable_surfel_dedup) {
		rg::add_compute(CSTR("Surfel: Dedup"),
						{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/surfel_dedup.comp")),
						 .macros = {vk::ShaderMacro("DEBUG_GRID_INVARIANTS", DEBUG_PASSES)},
						 .dims = {lm::div_ceil((u32)MAX_SURFEL_COUNT, (u32)ALLOCATE_PASS_WG_SIZE), 1, 1}})
			.push_constants(&pc)
			.bind({integrator->lumen_scene->scene_desc_buffer, integrator->scene_ubo_buffer});
	}

	if(DEBUG_PASSES) {
		rg::run_and_submit(cmd);

		u64 total_cells = get_total_grid_cells();

		_max_surfels_in_a_grid_cell = 0;
		_avg_surfels_in_a_grid_cell = 0;

		u32* counts = (u32*)vk::buffer_map(state.grid_cell_counts_buffer);

		u32 active_cells = 0;
		for(u64 i = 0; i < total_cells - 1; i++) {
			u32 count = counts[i + 1] - counts[i];
			if(count == 0) {
				continue;
			}
			active_cells++;
			_max_surfels_in_a_grid_cell = lm::max(_max_surfels_in_a_grid_cell, count);
			_avg_surfels_in_a_grid_cell += count;
		}
		_avg_surfels_in_a_grid_cell /= (f32)active_cells;
		vk::buffer_unmap(state.grid_cell_counts_buffer);
		pc.max_surfels_in_a_grid_cell = _highlight_max_surfel_cell ? _max_surfels_in_a_grid_cell : 0;

	}

	if (!state.pause_surfel_spawn) {
		u32 max_screen_tiles_x = lm::div_ceil(Window::width(), (u32)SURFELIZE_PASS_TILE_SIZE_XY);
		u32 max_screen_tiles_y = lm::div_ceil(Window::height(), (u32)SURFELIZE_PASS_TILE_SIZE_XY);
		rg::add_compute(CSTR("Surfel: Spawn"),
						{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/surfel_spawn.comp")),
						 .dims = {max_screen_tiles_x, max_screen_tiles_y, 1}})
			.push_constants(&pc)
			.zero(state.surfel_spawn_count_buffer)
			.bind({integrator->lumen_scene->scene_desc_buffer, integrator->scene_ubo_buffer});

		rg::add_compute(CSTR("Surfel: Allocate"),
						{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/surfel_allocate.comp")),
						 .dims = {lm::div_ceil(MAX_SURFEL_COUNT, ALLOCATE_PASS_WG_SIZE), 1, 1}})
			.push_constants(&pc)
			.bind({integrator->lumen_scene->scene_desc_buffer, integrator->scene_ubo_buffer});
	}

	rg::add_rt(CSTR("Surfel: Trace"),
			   {
				   .shaders = {{CSTR("src/shaders/integrators/irradiance_cache/surfel_trace.rgen")},
								   {CSTR("src/shaders/surface.rmiss")},
								   {CSTR("src/shaders/surface.rchit")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .dims = {(u32)state.rays_per_surfel, MAX_SURFEL_COUNT},
			   })
		.push_constants(&pc)
		.bind({integrator->output_tex, integrator->scene_ubo_buffer, integrator->lumen_scene->scene_desc_buffer,
			   integrator->lumen_scene->mesh_lights_buffer})
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.zero(state.surfel_samples_buffer)
		.bind_tlas(*integrator->tlas);

	rg::add_compute(CSTR("Surfel: Integrate"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/surfel_integrate.comp")),
					 .dims = {lm::div_ceil((u32)MAX_SURFEL_COUNT * state.rays_per_surfel, (u32)DEFAULT_WG_SIZE), 1, 1}})
		.push_constants(&pc)
		.bind({integrator->lumen_scene->scene_desc_buffer});

	if (state.debug_mode) {
		rg::add_compute(CSTR("Debug"),
						{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/debug.comp")),
						 .dims = {(u32)lm::ceil(Window::width() * Window::height() / f32(1024)), 1, 1}})
			.push_constants(&pc)
			.bind({integrator->lumen_scene->scene_desc_buffer, integrator->output_tex, integrator->scene_ubo_buffer})
			.bind_texture_array(integrator->lumen_scene->scene_textures);
	} else {
		u32 max_screen_tiles_x = lm::div_ceil(Window::width(), (u32)SURFELIZE_PASS_TILE_SIZE_XY);
		u32 max_screen_tiles_y = lm::div_ceil(Window::height(), (u32)SURFELIZE_PASS_TILE_SIZE_XY);
		rg::add_compute(CSTR("Composite"),
						{.shader = vk::Shader(CSTR("src/shaders/integrators/irradiance_cache/composite.comp")),
						 .dims = {max_screen_tiles_x, max_screen_tiles_y, 1}})
			.push_constants(&pc)
			.bind({integrator->lumen_scene->scene_desc_buffer, integrator->output_tex, integrator->scene_ubo_buffer})
			.bind_texture_array(integrator->lumen_scene->scene_textures);
	}
}

bool update(Integrator* integrator) {
	IrradianceCache& state = integrator->ircache;
	integrator->frame_num++;
	++state.total_frame_idx;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
	}
	return updated;
}

bool gui(Integrator* integrator) {
	IrradianceCache& state = integrator->ircache;
	PCIRCache& pc = state.pc;
	static const char* debug_view_display_names[IRCACHE_DEBUG_VIEW_COUNT] = {
		"GBuffer albedo",
		"GBuffer normals",
		"Grid",
		"Grid + surfels",
	};
	bool result = false;
	result |= ImGui::Checkbox("Direct lighting", &state.direct_lighting);
	result |= ImGui::Checkbox("Enable accumulation", &state.enable_accumulation);
	result |= ImGui::Checkbox("Debug mode", &state.debug_mode);
	ImGui::BeginDisabled(!state.debug_mode);
	if (ImGui::BeginCombo("Debug view", debug_view_display_names[state.debug_view])) {
		for (i32 n = 0; n < IRCACHE_DEBUG_VIEW_COUNT; n++) {
			const bool selected = state.debug_view == (u32)n;
			if (ImGui::Selectable(debug_view_display_names[n], selected)) {
				state.debug_view = (u32)n;
				result = true;
			}

			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	ImGui::EndDisabled();
	result |= ImGui::Checkbox("Pause surfel spawning", &state.pause_surfel_spawn);
	result |= ImGui::Checkbox("Camera relative surfel size", &state.use_camera_relative_surfel_size);
	result |= ImGui::Checkbox("Enable surfel deduplication", &state.enable_surfel_dedup);
	f32 max_trapezoidal_cell_size =
		get_px_size_per_trapezoidal_cell(integrator->scene_ubo.projection[1][1], (f32)Window::height());

	f32 max_allowed_surfel_radius =
		get_max_surfel_radius_px(integrator->scene_ubo.projection[1][1], (f32)Window::height());
	result |= ImGui::SliderFloat("Surfel radius (px)", &pc.desired_surfel_radius_px, 4.0f,
								 lm::floor(max_allowed_surfel_radius));
	result |=
		ImGui::SliderFloat("Uniform cell distance threshold", &pc.grid_uniform_cell_distance_threshold, 0.01f, 10.0f);
	result |= ImGui::SliderInt("Rays per surfel", (i32*)&state.rays_per_surfel, 0, 256);

	if (DEBUG_PASSES) {
		ImGui::NewLine();
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
		ImGui::Text("Debug Statistics:");
		ImGui::PopStyleColor();
		ImGui::Text("Max trapeoidal cell size (px): %u", (u32)max_trapezoidal_cell_size);
		ImGui::Text("Max surfels in a grid cell: %u\n", _max_surfels_in_a_grid_cell);
		ImGui::Text("Average surfels in a grid cell: %.2f\n", _avg_surfels_in_a_grid_cell);
		ImGui::Checkbox("Highlight fullest grid cell", &_highlight_max_surfel_cell);
	}
	return result;
}

void destroy(Integrator* integrator, bool resize) {
	IrradianceCache& state = integrator->ircache;
	(void)resize;

	vk::Buffer** buffers[] = {&state.gbuffer,
							  &state.current_frame_lighting_buffer,
							  &state.surfel_spawn_list_buffer,
							  &state.surfel_spawn_count_buffer,
							  &state.surfel_pool_buffer,
							  &state.surfel_free_stack_counter_buffer,
							  &state.surfel_free_stack_buffer,
							  &state.grid_cell_counts_buffer,
							  &state.grid_cell_indices_buffer,
							  &state.grid_prefix_sum_scratch_buffer,
							  &state.surfel_samples_buffer};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}
}

}  // namespace ircache
