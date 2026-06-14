#include "Integrator.h"
#include "PSSMLT.h"

namespace pssmlt {

void init(Integrator* integrator) {
	PSSMLT& state = integrator->pssmlt;

	u32 path_length = integrator->lumen_scene->config.common.path_length;
	state.light_path_rand_count = 6 + 3 * path_length;
	state.cam_path_rand_count = 2 + 3 * path_length;
	state.connect_path_rand_count = 4 * path_length;
	const PSSMLTConfig& config = integrator->lumen_scene->config.settings.pssmlt;

	// MLTVCM buffers
	state.bootstrap_buffer =
		prm::get_buffer({.name = CSTR("Bootstrap Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_bootstrap_samples * sizeof(BootstrapSample)});

	state.cdf_buffer =
		prm::get_buffer({.name = CSTR("CDF"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_bootstrap_samples * sizeof(f32)});

	state.bootstrap_cpu =
		prm::get_buffer({.name = CSTR("Boostrap - CPU"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU_TO_CPU,
						 .size = config.num_bootstrap_samples * sizeof(BootstrapSample)});

	state.cdf_cpu = prm::get_buffer({.name = CSTR("CDF - CPU"),
									 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									 .memory_type = vk::BUFFER_TYPE_GPU_TO_CPU,
									 .size = config.num_bootstrap_samples * sizeof(f32)});

	state.cdf_sum_buffer =
		prm::get_buffer({.name = CSTR("CDF Sums"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(f32)});

	state.seeds_buffer =
		prm::get_buffer({.name = CSTR("RNG Seeds"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * sizeof(SeedData)});

	state.light_primary_samples_buffer =
		prm::get_buffer({.name = CSTR("Primary Samples - Light"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * state.light_path_rand_count * sizeof(PrimarySample)});

	state.cam_primary_samples_buffer =
		prm::get_buffer({.name = CSTR("Primary Samples - Camera"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * state.cam_path_rand_count * sizeof(PrimarySample)});

	state.connection_primary_samples_buffer =
		prm::get_buffer({.name = CSTR("Primary Samples - Connection"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * state.connect_path_rand_count * sizeof(PrimarySample)});

	state.mlt_samplers_buffer =
		prm::get_buffer({.name = CSTR("MLT Samplers"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * sizeof(MLTSampler)});

	state.mlt_col_buffer =
		prm::get_buffer({.name = CSTR("MLT Color Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * 3 * sizeof(f32)});

	state.chain_stats_buffer =
		prm::get_buffer({.name = CSTR("Chain Stats"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * sizeof(ChainData)});

	state.splat_buffer =
		prm::get_buffer({.name = CSTR("Splat Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * (path_length * (path_length + 1)) * sizeof(Splat)});

	state.past_splat_buffer =
		prm::get_buffer({.name = CSTR("Past Splats Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * (path_length * (path_length + 1)) * sizeof(Splat)});

	u32 path_size = lm::max(config.num_mlt_threads, config.num_bootstrap_samples);

	state.light_path_buffer =
		prm::get_buffer({.name = CSTR("Light Paths"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = path_size * (path_length + 1) * sizeof(MLTPathVertex)});

	state.camera_path_buffer =
		prm::get_buffer({.name = CSTR("Camera Paths"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = path_size * (path_length + 1) * sizeof(MLTPathVertex)});

	i32 size = 0;
	i32 arr_size = config.num_bootstrap_samples;
	do {
		i32 num_blocks = lm::max(1, (i32)ceil(arr_size / (2.0f * 1024)));
		if (num_blocks > 1) {
			size++;
		}
		arr_size = num_blocks;
	} while (arr_size > 1);
	if (!state.block_sums.initialized()) {
		state.block_sums = lm::array_create<vk::Buffer*>(integrator->arena, size);
	}
	state.block_sums.resize(size);
	i32 i = 0;
	arr_size = config.num_bootstrap_samples;
	do {
		i32 num_blocks = lm::max(1, (i32)ceil(arr_size / (2.0f * 1024)));
		if (num_blocks > 1) {
			lm::String buffer_name = lm::str_concat(integrator->arena, "Block Sum Buffer #",
													lm::str_from_u64(integrator->arena, i), /*cstr=*/true);
			state.block_sums[i++] = prm::get_buffer(
				{.name = buffer_name,
				 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				 .memory_type = vk::BUFFER_TYPE_GPU,
				 .size = num_blocks * sizeof(f32)});
		}
		arr_size = num_blocks;
	} while (arr_size > 1);

	SceneDesc desc;
	desc.index_addr = integrator->lumen_scene->index_buffer->device_address();

	desc.material_addr = integrator->lumen_scene->materials_buffer->device_address();
	desc.prim_info_addr = integrator->lumen_scene->prim_lookup_buffer->device_address();
	desc.compact_vertices_addr = integrator->lumen_scene->vertex_buffer->device_address();
	// PSSMLT
	desc.bootstrap_addr = state.bootstrap_buffer->device_address();
	desc.cdf_addr = state.cdf_buffer->device_address();
	desc.cdf_sum_addr = state.cdf_sum_buffer->device_address();
	desc.seeds_addr = state.seeds_buffer->device_address();
	desc.light_primary_samples_addr = state.light_primary_samples_buffer->device_address();
	desc.cam_primary_samples_addr = state.cam_primary_samples_buffer->device_address();
	desc.connection_primary_samples_addr = state.connection_primary_samples_buffer->device_address();
	desc.mlt_samplers_addr = state.mlt_samplers_buffer->device_address();
	desc.mlt_col_addr = state.mlt_col_buffer->device_address();
	desc.chain_stats_addr = state.chain_stats_buffer->device_address();
	desc.splat_addr = state.splat_buffer->device_address();
	desc.past_splat_addr = state.past_splat_buffer->device_address();
	desc.light_path_addr = state.light_path_buffer->device_address();
	desc.camera_path_addr = state.camera_path_buffer->device_address();

	assert(vk::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, integrator->lumen_scene->prim_lookup_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, bootstrap_addr, state.bootstrap_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cdf_addr, state.cdf_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cdf_sum_addr, state.cdf_sum_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, seeds_addr, state.seeds_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_primary_samples_addr, state.light_primary_samples_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cam_primary_samples_addr, state.cam_primary_samples_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, connection_primary_samples_addr,
								 state.connection_primary_samples_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_samplers_addr, state.mlt_samplers_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_col_addr, state.mlt_col_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, chain_stats_addr, state.chain_stats_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, splat_addr, state.splat_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, past_splat_addr, state.past_splat_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_path_addr, state.light_path_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, camera_path_addr, state.camera_path_buffer, vk::render_graph());

	integrator->lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = CSTR("Scene Desc"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});
	state.pc.total_light_area = 0;

	integrator->frame_num = 0;

	state.mutation_count =
		i32(Window::width() * Window::height() * config.mutations_per_pixel / f32(config.num_mlt_threads));
	state.pc.mutations_per_pixel = config.mutations_per_pixel;
}

static void prefix_scan(Integrator* integrator, i32 level, i32 num_elems, i32& counter, lm::RenderGraph* rg) {
	PSSMLT& state = integrator->pssmlt;
	const bool scan_sums = level > 0;
	i32 num_wgs = lm::max(1, (i32)ceil(num_elems / (2 * 1024.0f)));
	i32 num_grids = num_wgs - i32((num_elems % 2048) != 0);
	state.pc_compute.num_elems = num_elems;
	auto scan = [&](i32 num_wgs, i32 idx) {
		++counter;
		rg->add_compute(CSTR("PrefixScan - Scan"),
						{.shader = vk::Shader(CSTR("src/shaders/integrators/pssmlt/prefix_scan.comp")),
						 .dims = {(u32)num_wgs, 1, 1}})
			.push_constants(&state.pc_compute)
			.bind(integrator->lumen_scene->scene_desc_buffer);
	};
	auto uniform_add = [&](i32 num_wgs, i32 output_idx) {
		++counter;
		rg->add_compute(CSTR("PrefixScan - Uniform Add"),
						{.shader = vk::Shader(CSTR("src/shaders/integrators/pssmlt/uniform_add.comp")),
						 .dims = {(u32)num_wgs, 1, 1}})
			.push_constants(&state.pc_compute)
			.bind(integrator->lumen_scene->scene_desc_buffer);
	};
	if (num_wgs > 1) {
		state.pc_compute.base_idx = 0;
		state.pc_compute.block_idx = 0;
		state.pc_compute.n = 2 * 1024;
		state.pc_compute.store_sum = 1;
		state.pc_compute.scan_sums = i32(scan_sums);
		state.pc_compute.block_sum_addr = state.block_sums[level]->device_address();
		scan(num_grids, level);
		i32 rem = num_elems % (2 * 1024);
		if (rem) {
			state.pc_compute.base_idx = num_elems - rem;
			state.pc_compute.block_idx = num_wgs - 1;
			state.pc_compute.n = rem;
			scan(1, level);
		}
		prefix_scan(integrator, level + 1, num_wgs, counter, rg);
		state.pc_compute.base_idx = 0;
		state.pc_compute.block_idx = 0;
		state.pc_compute.n = num_elems - rem;
		state.pc_compute.store_sum = 1;
		state.pc_compute.scan_sums = i32(scan_sums);
		state.pc_compute.block_sum_addr = state.block_sums[level]->device_address();
		if (scan_sums) {
			state.pc_compute.out_addr = state.block_sums[level - 1]->device_address();
		}
		uniform_add(num_grids, level - 1);
		if (rem) {
			state.pc_compute.base_idx = num_elems - rem;
			state.pc_compute.block_idx = num_wgs - 1;
			state.pc_compute.n = rem;
			uniform_add(1, level - 1);
		}
	} else {
		i32 rem = num_elems % 2048;
		state.pc_compute.n = rem == 0 ? 2048 : rem;
		state.pc_compute.base_idx = 0;
		state.pc_compute.block_idx = 0;
		state.pc_compute.store_sum = 0;
		state.pc_compute.scan_sums = bool(scan_sums);
		if (scan_sums) {
			state.pc_compute.block_sum_addr = state.block_sums[level - 1]->device_address();
		}
		scan(num_wgs, level - 1);
	}
}

void render(Integrator* integrator) {
	PSSMLT& state = integrator->pssmlt;
	const PSSMLTConfig& config = integrator->lumen_scene->config.settings.pssmlt;
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.num_lights = i32(integrator->lumen_scene->gpu_lights.size);
	state.pc.time = rand() % UINT_MAX;
	state.pc.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	// PSSMLT related constants
	state.pc.light_rand_count = state.light_path_rand_count;
	state.pc.cam_rand_count = state.cam_path_rand_count;
	state.pc.connection_rand_count = state.connect_path_rand_count;
	state.pc.random_num = rand() % UINT_MAX;
	state.pc.num_bootstrap_samples = config.num_bootstrap_samples;
	state.pc.total_light_area = integrator->lumen_scene->total_light_area;
	state.pc.total_light_count = integrator->lumen_scene->total_light_cnt;
	state.pc.frame_num = integrator->frame_num;

	std::initializer_list<lm::ResourceBinding> rt_bindings = {
		integrator->output_tex,
		integrator->scene_ubo_buffer,
		integrator->lumen_scene->scene_desc_buffer,
	};
	vk::CommandBuffer cmd(/*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	// Start bootstrap sampling
	vk::render_graph()
		->add_rt(CSTR("PSSMLT - Bootstrap Sampling"),
				 {
					 .shaders = {{CSTR("src/shaders/integrators/pssmlt/pssmlt_seed.rgen")},
								 {CSTR("src/shaders/ray.rmiss")},
								 {CSTR("src/shaders/ray_shadow.rmiss")},
								 {CSTR("src/shaders/ray.rchit")},
								 {CSTR("src/shaders/ray.rahit")}},
					 .specialization_data = {1},
					 .dims = {(u32)config.num_bootstrap_samples},
				 })
		.push_constants(&state.pc)
		.zero({state.light_path_buffer, state.camera_path_buffer})
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);

	i32 counter = 0;
	prefix_scan(integrator, 0, config.num_bootstrap_samples, counter, vk::render_graph());
	// Calculate CDF
	vk::render_graph()
		->add_compute(CSTR("Calculate CDF"),
					  {.shader = vk::Shader(CSTR("src/shaders/integrators/pssmlt/calc_cdf.comp")),
					   .specialization_data = {(u32)config.num_bootstrap_samples},
					   .dims = {(u32)lm::ceil(config.num_bootstrap_samples / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind(integrator->lumen_scene->scene_desc_buffer);

	lm::RenderGraph* rg = vk::render_graph();

	// Select seeds
	rg->add_compute(CSTR("Select Seeds"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/pssmlt/select_seeds.comp")),
					 .specialization_data = {(u32)config.num_mlt_threads},
					 .dims = {(u32)lm::ceil(config.num_mlt_threads / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind(integrator->lumen_scene->scene_desc_buffer);

	// Fill in the samplers for mutations
	rg->add_rt(CSTR("PSSMLT - Preprocess"),
			   {
				   .shaders = {{CSTR("src/shaders/integrators/pssmlt/pssmlt_preprocess.rgen")},
							   {CSTR("src/shaders/ray.rmiss")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rchit")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .dims = {(u32)config.num_mlt_threads},
			   })
		.push_constants(&state.pc)
		.zero({state.light_path_buffer, state.camera_path_buffer})
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);

	rg->run_and_submit(cmd);
	// Start mutations
	{
		auto mutate = [&](u32 i) {
			state.pc.random_num = rand() % UINT_MAX;
			state.pc.mutation_counter = i;
			rg->add_rt(CSTR("PSSMLT - Mutate"),
					   {
						   .shaders = {{CSTR("src/shaders/integrators/pssmlt/pssmlt_mutate.rgen")},
									   {CSTR("src/shaders/ray.rmiss")},
									   {CSTR("src/shaders/ray_shadow.rmiss")},
									   {CSTR("src/shaders/ray.rchit")},
									   {CSTR("src/shaders/ray.rahit")}},
						   .dims = {(u32)config.num_mlt_threads},
					   })
				.push_constants(&state.pc)
				.zero({state.light_path_buffer, state.camera_path_buffer})
				.bind(rt_bindings)
				.bind(integrator->lumen_scene->mesh_lights_buffer)
				.bind_texture_array(integrator->lumen_scene->scene_textures)
				.bind_tlas(*integrator->tlas);
		};
		const u32 iter_cnt = 100;
		const u32 freq = state.mutation_count / iter_cnt;
		i32 iter = 0;
		for (u32 f = 0; f < freq; f++) {
			cmd.begin();
			for (i32 i = 0; i < iter_cnt; i++) {
				mutate(i);
			}
			iter += 100;
			rg->run(cmd.handle);
			LUMEN_TRACE("%d / %d", iter, state.mutation_count);
			rg->submit(cmd);
		}
		const u32 rem = state.mutation_count % iter_cnt;
		if (rem) {
			cmd.begin();
			for (u32 i = 0; i < rem; i++) {
				mutate(i);
			}
			rg->run(cmd.handle);
			rg->submit(cmd);
		}
	}
	// Compositions
	rg->add_compute(CSTR("Composition"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/pssmlt/composite.comp")),
					 .dims = {(u32)lm::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind({integrator->output_tex, integrator->lumen_scene->scene_desc_buffer});
}

bool update(Integrator* integrator) {
	PSSMLT& state = integrator->pssmlt;
	integrator->frame_num++;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
	}
	return updated;
}

void destroy(Integrator* integrator, bool resize) {
	PSSMLT& state = integrator->pssmlt;
	(void)resize;

	vk::Buffer** buffers[] = {&state.bootstrap_buffer,
							  &state.cdf_buffer,
							  &state.cdf_sum_buffer,
							  &state.seeds_buffer,
							  &state.mlt_samplers_buffer,
							  &state.light_primary_samples_buffer,
							  &state.cam_primary_samples_buffer,
							  &state.connection_primary_samples_buffer,
							  &state.mlt_col_buffer,
							  &state.chain_stats_buffer,
							  &state.splat_buffer,
							  &state.past_splat_buffer,
							  &state.light_path_buffer,
							  &state.camera_path_buffer,
							  &state.bootstrap_cpu,
							  &state.cdf_cpu};
	for (vk::Buffer** buffer : buffers) {
		prm::remove(*buffer);
		*buffer = nullptr;
	}
	for (vk::Buffer*& buffer : state.block_sums) {
		prm::remove(buffer);
		buffer = nullptr;
	}
	state.block_sums.clear();
}

}  // namespace pssmlt
