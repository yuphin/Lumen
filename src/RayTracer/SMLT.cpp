#include "Integrator.h"
#include "Framework/RenderGraph.h"
#include "SMLT.h"

namespace smlt {

void init(Integrator* integrator) {
	SMLT& state = integrator->smlt;

	const SMLTConfig& config = integrator->lumen_scene->config.settings.smlt;
	state.mutations_per_pixel = config.mutations_per_pixel;
	state.num_mlt_threads = config.num_mlt_threads;
	state.num_bootstrap_samples = config.num_bootstrap_samples;
	state.mutation_count =
		i32(Window::width() * Window::height() * state.mutations_per_pixel / f32(state.num_mlt_threads));
	u32 path_length = integrator->lumen_scene->config.common.path_length;
	state.light_path_rand_count = 6 + 3 * path_length;
	state.cam_path_rand_count = 3 + 7 * path_length;

	state.bootstrap_buffer =
		prm::get_buffer({.name = CSTR("Bootstrap Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = state.num_bootstrap_samples * sizeof(BootstrapSample)});

	state.cdf_buffer =
		prm::get_buffer({.name = CSTR("CDF Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = VkDeviceSize(state.num_bootstrap_samples * 4)});

	state.bootstrap_cpu =
		prm::get_buffer({.name = CSTR("Bootstrap CPU"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU_TO_CPU,
						 .size = state.num_bootstrap_samples * sizeof(BootstrapSample)});

	state.cdf_cpu = prm::get_buffer({.name = CSTR("CDF CPU"),
									 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									 .memory_type = vk::BUFFER_TYPE_GPU_TO_CPU,
									 .size = VkDeviceSize(state.num_bootstrap_samples * 4)});

	state.cdf_sum_buffer =
		prm::get_buffer({.name = CSTR("CDF Sum Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(f32)});

	state.seeds_buffer =
		prm::get_buffer({.name = CSTR("Seeds Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = state.num_mlt_threads * sizeof(SeedData)});

	state.light_primary_samples_buffer =
		prm::get_buffer({.name = CSTR("Light Primary Samples Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = state.num_mlt_threads * state.light_path_rand_count * sizeof(PrimarySample)});

	state.cam_primary_samples_buffer =
		prm::get_buffer({.name = CSTR("Cam Primary Samples Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = state.num_mlt_threads * state.cam_path_rand_count * sizeof(PrimarySample)});

	state.mlt_samplers_buffer =
		prm::get_buffer({.name = CSTR("MLT Samplers Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = state.num_mlt_threads * sizeof(MLTSampler)});

	state.mlt_col_buffer =
		prm::get_buffer({.name = CSTR("MLT Col Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * 3 * sizeof(f32)});

	state.chain_stats_buffer =
		prm::get_buffer({.name = CSTR("Chain Stats Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = state.num_mlt_threads * sizeof(ChainData)});

	state.splat_buffer =
		prm::get_buffer({.name = CSTR("Splat Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = state.num_mlt_threads * (path_length * (path_length + 1)) * sizeof(Splat)});

	state.past_splat_buffer =
		prm::get_buffer({.name = CSTR("Past Splat Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = state.num_mlt_threads * (path_length * (path_length + 1)) * sizeof(Splat)});

	u32 path_size = lm::max(state.num_mlt_threads, state.num_bootstrap_samples);
	state.light_path_buffer =
		prm::get_buffer({.name = CSTR("Light Path Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = path_size * (path_length + 1) * sizeof(VCMVertex)});

	state.connected_lights_buffer =
		prm::get_buffer({.name = CSTR("Connected Lights Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = path_size * sizeof(u32)});

	state.tmp_seeds_buffer =
		prm::get_buffer({.name = CSTR("Tmp Seeds Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = path_size * sizeof(SeedData)});

	state.light_path_cnt_buffer =
		prm::get_buffer({.name = CSTR("Light Path Count Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = path_size * sizeof(f32)});

	state.light_splats_buffer =
		prm::get_buffer({.name = CSTR("Light Splats Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = path_size * (path_length * (path_length + 1)) * sizeof(Splat)});

	state.light_splat_cnts_buffer =
		prm::get_buffer({.name = CSTR("Light Splat Counts Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = path_size * sizeof(f32)});

	state.tmp_lum_buffer =
		prm::get_buffer({.name = CSTR("Tmp Lum Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = state.num_bootstrap_samples * sizeof(f32)});

	state.prob_carryover_buffer =
		prm::get_buffer({.name = CSTR("Prob Carryover Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = state.num_mlt_threads * sizeof(u32)});

	i32 size = 0;
	i32 arr_size = state.num_bootstrap_samples;
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
	arr_size = state.num_bootstrap_samples;
	do {
		i32 num_blocks = lm::max(1, (i32)ceil(arr_size / (2.0f * 1024)));
		if (num_blocks > 1) {
			lm::String buffer_name = lm::str_concat(integrator->arena, "Block Sum Buffer #",
													lm::str_from_u64(integrator->arena, i), /*cstr=*/true);
			state.block_sums[i++] = prm::get_buffer(
				{.name = buffer_name,
				 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				 .memory_type = vk::BUFFER_TYPE_GPU,
				 .size = VkDeviceSize(num_blocks * 4)});
		}
		arr_size = num_blocks;
	} while (arr_size > 1);

	SceneDesc desc;
	desc.index_addr = integrator->lumen_scene->index_buffer->device_address();

	desc.material_addr = integrator->lumen_scene->materials_buffer->device_address();
	desc.prim_info_addr = integrator->lumen_scene->prim_lookup_buffer->device_address();
	desc.compact_vertices_addr = integrator->lumen_scene->vertex_buffer->device_address();
	// SMLT
	desc.bootstrap_addr = state.bootstrap_buffer->device_address();
	desc.cdf_addr = state.cdf_buffer->device_address();
	desc.cdf_sum_addr = state.cdf_sum_buffer->device_address();
	desc.seeds_addr = state.seeds_buffer->device_address();
	desc.light_primary_samples_addr = state.light_primary_samples_buffer->device_address();
	desc.cam_primary_samples_addr = state.cam_primary_samples_buffer->device_address();
	desc.mlt_samplers_addr = state.mlt_samplers_buffer->device_address();
	desc.mlt_col_addr = state.mlt_col_buffer->device_address();
	desc.chain_stats_addr = state.chain_stats_buffer->device_address();
	desc.splat_addr = state.splat_buffer->device_address();
	desc.past_splat_addr = state.past_splat_buffer->device_address();
	desc.vcm_vertices_addr = state.light_path_buffer->device_address();
	desc.connected_lights_addr = state.connected_lights_buffer->device_address();
	desc.tmp_seeds_addr = state.tmp_seeds_buffer->device_address();
	desc.path_cnt_addr = state.light_path_cnt_buffer->device_address();
	desc.tmp_lum_addr = state.tmp_lum_buffer->device_address();
	desc.prob_carryover_addr = state.prob_carryover_buffer->device_address();
	desc.light_splats_addr = state.light_splats_buffer->device_address();
	desc.light_splat_cnts_addr = state.light_splat_cnts_buffer->device_address();
	assert(rg::settings().shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, integrator->lumen_scene->prim_lookup_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, bootstrap_addr, state.bootstrap_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cdf_addr, state.cdf_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cdf_sum_addr, state.cdf_sum_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, seeds_addr, state.seeds_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_primary_samples_addr, state.light_primary_samples_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cam_primary_samples_addr, state.cam_primary_samples_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_samplers_addr, state.mlt_samplers_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_col_addr, state.mlt_col_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, chain_stats_addr, state.chain_stats_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, splat_addr, state.splat_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, past_splat_addr, state.past_splat_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, vcm_vertices_addr, state.light_path_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, connected_lights_addr, state.connected_lights_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, tmp_seeds_addr, state.tmp_seeds_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, path_cnt_addr, state.light_path_cnt_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, tmp_lum_addr, state.tmp_lum_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prob_carryover_addr, state.prob_carryover_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_splats_addr, state.light_splats_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_splat_cnts_addr, state.light_splat_cnts_buffer);

	integrator->lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = CSTR("Scene Desc"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});

	state.pc.total_light_area = 0;

	integrator->frame_num = 0;
	state.pc.mutations_per_pixel = state.mutations_per_pixel;
	state.pc.use_vc = 1;
	state.pc.use_vm = 0;
}

static void prefix_scan(Integrator* integrator, i32 level, i32 num_elems, i32& counter) {
	SMLT& state = integrator->smlt;
	const bool scan_sums = level > 0;
	i32 num_wgs = lm::max(1, (i32)ceil(num_elems / (2 * 1024.0f)));
	i32 num_grids = num_wgs - i32((num_elems % 2048) != 0);
	state.pc_compute.num_elems = num_elems;
	auto scan = [&](i32 num_wgs, i32 idx) {
		++counter;
		rg::add_compute(CSTR("PrefixScan - Scan"),
						{.shader = vk::Shader(CSTR("src/shaders/integrators/pssmlt/prefix_scan.comp")),
						 .dims = {(u32)num_wgs, 1, 1}})
			.push_constants(&state.pc_compute)
			.bind(integrator->lumen_scene->scene_desc_buffer);
	};
	auto uniform_add = [&](i32 num_wgs, i32 output_idx) {
		++counter;
		rg::add_compute(CSTR("PrefixScan - Uniform Add"),
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
		prefix_scan(integrator, level + 1, num_wgs, counter);
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
	SMLT& state = integrator->smlt;
	vk::CommandBuffer cmd(/*start*/ true);
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.num_lights = i32(integrator->lumen_scene->gpu_lights.size);
	state.pc.time = rand() % U32_MAX;
	state.pc.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	// SMLT related constants
	state.pc.light_rand_count = state.light_path_rand_count;
	state.pc.cam_rand_count = state.cam_path_rand_count;
	state.pc.random_num = rand() % U32_MAX;
	state.pc.num_bootstrap_samples = state.num_bootstrap_samples;
	state.pc.total_light_area = integrator->lumen_scene->total_light_area;
	state.pc.total_light_count = integrator->lumen_scene->total_light_cnt;
	state.pc.frame_num = integrator->frame_num;

	const std::initializer_list<lm::ResourceBinding> rt_bindings = {
		integrator->output_tex,
		integrator->scene_ubo_buffer,
		integrator->lumen_scene->scene_desc_buffer,
	};

	// Start bootstrap sampling
	{
		// Light
		rg::add_rt(CSTR("SMLT - Bootstrap Sampling - Light"),
				   {
					   .shaders = {{CSTR("src/shaders/integrators/smlt/smlt_seed_light.rgen")},
								   {CSTR("src/shaders/ray.rmiss")},
								   {CSTR("src/shaders/ray_shadow.rmiss")},
								   {CSTR("src/shaders/ray.rchit")},
								   {CSTR("src/shaders/ray.rahit")}},
					   .specialization_data = {1},
					   .dims = {(u32)state.num_bootstrap_samples},
				   })
			.push_constants(&state.pc)
			.bind(rt_bindings)
			.bind(integrator->lumen_scene->mesh_lights_buffer)
			.bind_texture_array(integrator->lumen_scene->scene_textures)
			.bind_tlas(*integrator->tlas);
		// Eye
		rg::add_rt(CSTR("SMLT - Bootstrap Sampling - Eye"),
				   {
					   .shaders = {{CSTR("src/shaders/integrators/smlt/smlt_seed_eye.rgen")},
								   {CSTR("src/shaders/ray.rmiss")},
								   {CSTR("src/shaders/ray_shadow.rmiss")},
								   {CSTR("src/shaders/ray.rchit")},
								   {CSTR("src/shaders/ray.rahit")}},
					   .specialization_data = {1},
					   .dims = {(u32)state.num_bootstrap_samples},
				   })
			.push_constants(&state.pc)
			.bind(rt_bindings)
			.bind(integrator->lumen_scene->mesh_lights_buffer)
			.bind_texture_array(integrator->lumen_scene->scene_textures)
			.bind_tlas(*integrator->tlas);
	}
	i32 counter = 0;
	prefix_scan(integrator, 0, integrator->lumen_scene->config.settings.smlt.num_bootstrap_samples, counter);
	// Calculate CDF
	rg::add_compute(CSTR("Calculate CDF"), {.shader = vk::Shader(CSTR("src/shaders/integrators/pssmlt/calc_cdf.comp")),
											.specialization_data = {(u32)state.num_bootstrap_samples},
											.dims = {(u32)lm::ceil(state.num_bootstrap_samples / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind(integrator->lumen_scene->scene_desc_buffer);
	// Select seeds
	rg::add_compute(CSTR("Select Seeds"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/pssmlt/select_seeds.comp")),
					 .specialization_data = {(u32)state.num_mlt_threads},
					 .dims = {(u32)lm::ceil(state.num_mlt_threads / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind(integrator->lumen_scene->scene_desc_buffer);
	// Fill in the samplers for mutations
	{
		// Light
		rg::add_rt(CSTR("SMLT - Preprocess - Light"),
				   {
					   .shaders = {{CSTR("src/shaders/integrators/smlt/smlt_preprocess_light.rgen")},
								   {CSTR("src/shaders/ray.rmiss")},
								   {CSTR("src/shaders/ray_shadow.rmiss")},
								   {CSTR("src/shaders/ray.rchit")},
								   {CSTR("src/shaders/ray.rahit")}},
					   .dims = {(u32)state.num_mlt_threads},
				   })
			.push_constants(&state.pc)
			.zero({state.mlt_samplers_buffer, state.light_primary_samples_buffer, state.cam_primary_samples_buffer})
			.bind(rt_bindings)
			.bind(integrator->lumen_scene->mesh_lights_buffer)
			.bind_texture_array(integrator->lumen_scene->scene_textures)
			.bind_tlas(*integrator->tlas);
		// Eye
		rg::add_rt(CSTR("SMLT - Preprocess - Eye"),
				   {
					   .shaders = {{CSTR("src/shaders/integrators/smlt/smlt_preprocess_eye.rgen")},
								   {CSTR("src/shaders/ray.rmiss")},
								   {CSTR("src/shaders/ray_shadow.rmiss")},
								   {CSTR("src/shaders/ray.rchit")},
								   {CSTR("src/shaders/ray.rahit")}},
					   .dims = {(u32)state.num_mlt_threads},
				   })
			.push_constants(&state.pc)
			.bind(rt_bindings)
			.bind(integrator->lumen_scene->mesh_lights_buffer)
			.bind_texture_array(integrator->lumen_scene->scene_textures)
			.bind_tlas(*integrator->tlas);
	}
	rg::run_and_submit(cmd);
	// Start mutations
	{
		auto mutate = [&](u32 i) {
			state.pc.random_num = rand() % U32_MAX;
			state.pc.mutation_counter = i;
			// Light
			rg::add_rt(CSTR("PSSMLT - Mutate - Light"),
					   {
						   .shaders = {{CSTR("src/shaders/integrators/smlt/smlt_mutate_light.rgen")},
									   {CSTR("src/shaders/ray.rmiss")},
									   {CSTR("src/shaders/ray_shadow.rmiss")},
									   {CSTR("src/shaders/ray.rchit")},
									   {CSTR("src/shaders/ray.rahit")}},
						   .dims = {(u32)state.num_mlt_threads},
					   })
				.push_constants(&state.pc)
				.bind(rt_bindings)
				.bind(integrator->lumen_scene->mesh_lights_buffer)
				.bind_texture_array(integrator->lumen_scene->scene_textures)
				.bind_tlas(*integrator->tlas);
			// Eye
			rg::add_rt(CSTR("PSSMLT - Mutate - Eye"),
					   {
						   .shaders = {{CSTR("src/shaders/integrators/smlt/smlt_mutate_eye.rgen")},
									   {CSTR("src/shaders/ray.rmiss")},
									   {CSTR("src/shaders/ray_shadow.rmiss")},
									   {CSTR("src/shaders/ray.rchit")},
									   {CSTR("src/shaders/ray.rahit")}},
						   .dims = {(u32)state.num_mlt_threads},
					   })
				.push_constants(&state.pc)
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
			rg::run(cmd.handle);
			LUMEN_TRACE("%d / %d", iter, state.mutation_count);
			rg::submit(cmd);
		}
		const u32 rem = state.mutation_count % iter_cnt;
		if (rem) {
			cmd.begin();
			for (u32 i = 0; i < rem; i++) {
				mutate(i);
			}
			rg::run(cmd.handle);
			rg::submit(cmd);
		}
	}
	// Compositions
	rg::add_compute(CSTR("Composition"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/pssmlt/composite.comp")),
					 .dims = {(u32)lm::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind({integrator->output_tex, integrator->lumen_scene->scene_desc_buffer});
}

bool update(Integrator* integrator) {
	SMLT& state = integrator->smlt;
	integrator->frame_num++;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
	}
	return updated;
}

void destroy(Integrator* integrator, bool resize) {
	SMLT& state = integrator->smlt;
	(void)resize;

	vk::Buffer** buffers[] = {&state.bootstrap_buffer,
							  &state.cdf_buffer,
							  &state.cdf_sum_buffer,
							  &state.seeds_buffer,
							  &state.mlt_samplers_buffer,
							  &state.light_primary_samples_buffer,
							  &state.cam_primary_samples_buffer,
							  &state.mlt_col_buffer,
							  &state.chain_stats_buffer,
							  &state.splat_buffer,
							  &state.past_splat_buffer,
							  &state.light_path_buffer,
							  &state.connected_lights_buffer,
							  &state.tmp_seeds_buffer,
							  &state.tmp_lum_buffer,
							  &state.prob_carryover_buffer,
							  &state.light_path_cnt_buffer,
							  &state.light_splats_buffer,
							  &state.light_splat_cnts_buffer,
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

}  // namespace smlt
