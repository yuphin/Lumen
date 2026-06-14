#include "Integrator.h"
#include "Framework/RenderGraph.h"
#include "VCMMLT.h"
namespace vcmmlt {

void init(Integrator* integrator) {
	VCMMLT& state = integrator->vcmmlt;

	const VCMMLTConfig& config = integrator->lumen_scene->config.settings.vcmmlt;
	u32 path_length = integrator->lumen_scene->config.common.path_length;
	state.mutation_count =
		i32(Window::width() * Window::height() * config.mutations_per_pixel / f32(config.num_mlt_threads));
	state.light_path_rand_count = lm::max(7 + 3 * path_length, 3 + 7 * path_length);

	// MLTVCM buffers
	state.bootstrap_buffer =
		prm::get_buffer({.name = CSTR("Bootstrap Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_bootstrap_samples * sizeof(BootstrapSample)});

	state.cdf_buffer =
		prm::get_buffer({.name = CSTR("CDF Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = VkDeviceSize(config.num_bootstrap_samples * 4)});

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
						 .size = config.num_mlt_threads * sizeof(VCMMLTSeedData)});

	state.light_primary_samples_buffer =
		prm::get_buffer({.name = CSTR("Light Primary Samples Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * state.light_path_rand_count * sizeof(PrimarySample) * 2});

	state.mlt_samplers_buffer =
		prm::get_buffer({.name = CSTR("MLT Samplers Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * sizeof(VCMMLTSampler) * 2});

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
						 .size = 2 * sizeof(ChainData)});

	state.splat_buffer =
		prm::get_buffer({.name = CSTR("Splat Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * (path_length * (path_length + 1)) * sizeof(Splat) * 2});

	state.past_splat_buffer =
		prm::get_buffer({.name = CSTR("Past Splat Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * (path_length * (path_length + 1)) * sizeof(Splat) * 2});

	state.light_path_buffer =
		prm::get_buffer({.name = CSTR("Light Path Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * (path_length + 1) * sizeof(VCMVertex)});

	state.light_path_cnt_buffer =
		prm::get_buffer({.name = CSTR("Light Path Cnt Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(f32)});

	state.tmp_col_buffer =
		prm::get_buffer({.name = CSTR("Tmp Col Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(f32) * 3});

	state.photon_buffer =
		prm::get_buffer({.name = CSTR("Photon Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = 10 * Window::width() * Window::height() * sizeof(VCMPhotonHash)});

	state.mlt_atomicsum_buffer =
		prm::get_buffer({.name = CSTR("MLT Atomic Sum Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * sizeof(SumData) * 2});

	state.mlt_residual_buffer =
		prm::get_buffer({.name = CSTR("MLT Residual Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * sizeof(SumData)});

	state.counter_buffer =
		prm::get_buffer({.name = CSTR("Counter Buffer"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(i32)});

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
				 .size = VkDeviceSize(num_blocks * 4)});
		}
		arr_size = num_blocks;
	} while (arr_size > 1);

	SceneDesc desc;
	desc.index_addr = integrator->lumen_scene->index_buffer->device_address();

	desc.material_addr = integrator->lumen_scene->materials_buffer->device_address();
	desc.prim_info_addr = integrator->lumen_scene->prim_lookup_buffer->device_address();
	desc.compact_vertices_addr = integrator->lumen_scene->vertex_buffer->device_address();
	// VCMMLT
	desc.bootstrap_addr = state.bootstrap_buffer->device_address();
	desc.cdf_addr = state.cdf_buffer->device_address();
	desc.cdf_sum_addr = state.cdf_sum_buffer->device_address();
	desc.seeds_addr = state.seeds_buffer->device_address();
	desc.light_primary_samples_addr = state.light_primary_samples_buffer->device_address();
	desc.mlt_samplers_addr = state.mlt_samplers_buffer->device_address();
	desc.mlt_col_addr = state.mlt_col_buffer->device_address();
	desc.chain_stats_addr = state.chain_stats_buffer->device_address();
	desc.splat_addr = state.splat_buffer->device_address();
	desc.past_splat_addr = state.past_splat_buffer->device_address();

	desc.vcm_vertices_addr = state.light_path_buffer->device_address();
	desc.path_cnt_addr = state.light_path_cnt_buffer->device_address();

	desc.color_storage_addr = state.tmp_col_buffer->device_address();
	desc.photon_addr = state.photon_buffer->device_address();

	desc.mlt_atomicsum_addr = state.mlt_atomicsum_buffer->device_address();
	desc.residual_addr = state.mlt_residual_buffer->device_address();
	desc.counter_addr = state.counter_buffer->device_address();

	assert(rg::settings().shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, integrator->lumen_scene->prim_lookup_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, bootstrap_addr, state.bootstrap_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cdf_addr, state.cdf_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cdf_sum_addr, state.cdf_sum_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, seeds_addr, state.seeds_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_primary_samples_addr, state.light_primary_samples_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_samplers_addr, state.mlt_samplers_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_col_addr, state.mlt_col_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, chain_stats_addr, state.chain_stats_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, splat_addr, state.splat_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, past_splat_addr, state.past_splat_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, vcm_vertices_addr, state.light_path_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, path_cnt_addr, state.light_path_cnt_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, state.tmp_col_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, photon_addr, state.photon_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_atomicsum_addr, state.mlt_atomicsum_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, residual_addr, state.mlt_residual_buffer);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, counter_addr, state.counter_buffer);

	integrator->lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = CSTR("Scene Desc"),
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});
	state.pc.total_light_area = 0;

	integrator->frame_num = 0;

	state.pc.mutations_per_pixel = config.mutations_per_pixel;
	state.pc.num_mlt_threads = config.num_mlt_threads;
}

static void prefix_scan(Integrator* integrator, i32 level, i32 num_elems, i32& counter) {
	VCMMLT& state = integrator->vcmmlt;
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
	VCMMLT& state = integrator->vcmmlt;
	LUMEN_TRACE("Rendering sample %d...", state.sample_cnt++);
	vk::CommandBuffer cmd(/*start*/ true);
	const VCMMLTConfig& config = integrator->lumen_scene->config.settings.vcmmlt;
	state.pc.width = Window::width();
	state.pc.height = Window::height();
	state.pc.num_lights = i32(integrator->lumen_scene->gpu_lights.size);
	state.pc.time = rand() % UINT_MAX;
	state.pc.max_depth = integrator->lumen_scene->config.common.path_length;
	state.pc.sky_col = integrator->lumen_scene->config.common.sky_col;
	state.pc.frame_num = integrator->frame_num;
	// VCMMLT related constants
	state.pc.use_vm = config.enable_vm;
	state.pc.light_rand_count = state.light_path_rand_count;
	state.pc.random_num = rand() % UINT_MAX;
	state.pc.num_bootstrap_samples = config.num_bootstrap_samples;
	state.pc.radius = integrator->lumen_scene->dimensions.radius * config.radius_factor / 100.f;
	state.pc.radius /= (f32)pow((double)state.pc.frame_num + 1, 0.5 * (1 - 2.0 / 3));
	state.pc.min_bounds = integrator->lumen_scene->dimensions.min;
	state.pc.max_bounds = integrator->lumen_scene->dimensions.max;
	const lm::vec3 diam = state.pc.max_bounds - state.pc.min_bounds;
	const f32 max_comp = lm::max(diam.x, lm::max(diam.y, diam.z));
	const i32 base_grid_res = i32(max_comp / state.pc.radius);
	state.pc.grid_res = lm::max(ivec3(diam * f32(base_grid_res) / max_comp), ivec3(1));
	state.pc.total_light_area = integrator->lumen_scene->total_light_area;
	state.pc.total_light_count = integrator->lumen_scene->total_light_cnt;
	auto op_reduce = [&](const lm::String& op_name, const lm::String& op_shader_name, const lm::String& reduce_name,
						 const lm::String& reduce_shader_name, const lm::SpecializationConstantArray& spec_data) {
		u32 num_wgs = u32((config.num_mlt_threads + 1023) / 1024);
		rg::add_compute(
			  op_name,
			  {.shader = vk::Shader(op_shader_name), .specialization_data = spec_data, .dims = {num_wgs, 1, 1}})
			.push_constants(&state.pc)
			.bind(integrator->lumen_scene->scene_desc_buffer)
			.zero({state.mlt_residual_buffer, state.counter_buffer});
		while (num_wgs != 1) {
			rg::add_compute(
				  reduce_name,
				  {.shader = vk::Shader(reduce_shader_name), .specialization_data = spec_data, .dims = {num_wgs, 1, 1}})
				.push_constants(&state.pc)
				.bind(integrator->lumen_scene->scene_desc_buffer);
			num_wgs = (u32)(num_wgs + 1023) / 1024;
		}
	};
	auto sum_up_chain_data = [&] {
		op_reduce(CSTR("OpReduce: Sum0"), CSTR("src/shaders/integrators/vcmmlt/sum.comp"),
				  CSTR("OpReduce: Reduce Sum0"), CSTR("src/shaders/integrators/vcmmlt/reduce_sum.comp"), {0});
		op_reduce(CSTR("OpReduce: Sum1"), CSTR("src/shaders/integrators/vcmmlt/sum.comp"),
				  CSTR("OpReduce: Reduce Sum1"), CSTR("src/shaders/integrators/vcmmlt/reduce_sum.comp"), {1});
	};
	std::initializer_list<lm::ResourceBinding> rt_bindings = {
		integrator->output_tex,
		integrator->scene_ubo_buffer,
		integrator->lumen_scene->scene_desc_buffer,
	};
	lm::SpecializationConstantArray spec_consts;
	if (!config.light_first) {
		spec_consts = {1, 0};
	} else {
		spec_consts = {1, 1};
	}
	lm::String pipeline_postfix = config.light_first ? lm::String("-SEED&LIGHT_FIRST") : lm::String("-SEED");
	// Shoot rays
	lm::String pipeline_name = lm::str_concat(rg::arena(), "VCMMLT - Trace ", pipeline_postfix, /*cstr=*/true);
	rg::add_rt(pipeline_name,
			   {
				   .shaders = {{CSTR("src/shaders/integrators/vcmmlt/vcmmlt_eye.rgen")},
							   {CSTR("src/shaders/ray.rmiss")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rchit")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .specialization_data = spec_consts,
				   .dims = {Window::width() * Window::height()},
			   })
		.push_constants(&state.pc)
		.zero({state.chain_stats_buffer, state.mlt_atomicsum_buffer})
		.zero(state.photon_buffer, config.enable_vm)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	// Start bootstrap sampling
	pipeline_name = lm::str_concat(rg::arena(), "VCMMLT - Bootstrap ", pipeline_postfix, /*cstr=*/true);
	rg::add_rt(pipeline_name,
			   {
				   .shaders = {{CSTR("src/shaders/integrators/vcmmlt/vcmmlt_seed.rgen")},
							   {CSTR("src/shaders/ray.rmiss")},
							   {CSTR("src/shaders/ray_shadow.rmiss")},
							   {CSTR("src/shaders/ray.rchit")},
							   {CSTR("src/shaders/ray.rahit")}},
				   .specialization_data = spec_consts,
				   .dims = {(u32)config.num_bootstrap_samples},
			   })
		.push_constants(&state.pc)
		.bind(rt_bindings)
		.bind(integrator->lumen_scene->mesh_lights_buffer)
		.bind_texture_array(integrator->lumen_scene->scene_textures)
		.bind_tlas(*integrator->tlas);
	i32 counter = 0;
	prefix_scan(integrator, 0, config.num_bootstrap_samples, counter);
	// Calculate CDF
	rg::add_compute(CSTR("Calculate CDF"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/pssmlt/calc_cdf.comp")),
					 .dims = {(u32)lm::ceil(config.num_bootstrap_samples / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind(integrator->lumen_scene->scene_desc_buffer);
	// Select seeds
	rg::add_compute(CSTR("Select Seeds"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/vcmmlt/select_seeds.comp")),
					 .dims = {(u32)lm::ceil(config.num_mlt_threads / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind(integrator->lumen_scene->scene_desc_buffer);
	// Fill in the samplers for mutations
	{
		// Fill
		lm::String preprocess_name =
			lm::str_concat(rg::arena(), "VCMMLT - Preprocess ", pipeline_postfix, /*cstr=*/true);
		rg::add_rt(preprocess_name,
				   {
					   .shaders = {{CSTR("src/shaders/integrators/vcmmlt/vcmmlt_preprocess.rgen")},
								   {CSTR("src/shaders/ray.rmiss")},
								   {CSTR("src/shaders/ray_shadow.rmiss")},
								   {CSTR("src/shaders/ray.rchit")},
								   {CSTR("src/shaders/ray.rahit")}},
					   .specialization_data = spec_consts,
					   .dims = {(u32)config.num_mlt_threads},
				   })
			.push_constants(&state.pc)
			.bind(rt_bindings)
			.bind(integrator->lumen_scene->mesh_lights_buffer)
			.bind_texture_array(integrator->lumen_scene->scene_textures)
			.bind_tlas(*integrator->tlas);
		// Sum up chain stats
		sum_up_chain_data();
	}
	// Calculate normalization factor
	rg::add_compute(CSTR("Calculate Normalization"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/vcmmlt/normalize.comp")), .dims = {1, 1, 1}})
		.push_constants(&state.pc)
		.bind(integrator->lumen_scene->scene_desc_buffer);
	rg::run_and_submit(cmd);
	// Start mutations
	{
		lm::String mutate_name = lm::str_concat(rg::arena(), "VCMMLT - Mutate ", pipeline_postfix, /*cstr=*/true);
		auto mutate = [&](u32 i) {
			state.pc.random_num = rand() % UINT_MAX;
			state.pc.mutation_counter = i;
			// Mutate
			rg::add_rt(mutate_name,
					   {
						   .shaders = {{CSTR("src/shaders/integrators/vcmmlt/vcmmlt_mutate.rgen")},
									   {CSTR("src/shaders/ray.rmiss")},
									   {CSTR("src/shaders/ray_shadow.rmiss")},
									   {CSTR("src/shaders/ray.rchit")},
									   {CSTR("src/shaders/ray.rahit")}},
						   .dims = {(u32)config.num_mlt_threads},
					   })
				.push_constants(&state.pc)
				.zero(state.mlt_atomicsum_buffer)
				.bind(rt_bindings)
				.bind(integrator->lumen_scene->mesh_lights_buffer)
				.bind_texture_array(integrator->lumen_scene->scene_textures)
				.bind_tlas(*integrator->tlas);
			sum_up_chain_data();
			// Normalization
			rg::add_compute(
				  CSTR("Calculate Normalization"),
				  {.shader = vk::Shader(CSTR("src/shaders/integrators/vcmmlt/normalize.comp")), .dims = {1, 1, 1}})
				.push_constants(&state.pc)
				.bind(integrator->lumen_scene->scene_desc_buffer);
		};
		const u32 iter_cnt = 100;
		const u32 freq = state.mutation_count / iter_cnt;
		u32 cnt = 0;
		for (u32 f = 0; f < freq; f++) {
			LUMEN_TRACE("Mutation: %d / %d", cnt, state.mutation_count);
			cmd.begin();
			for (i32 i = 0; i < iter_cnt; i++) {
				mutate(cnt++);
			}
			rg::run(cmd.handle);
			rg::submit(cmd);
		}
		const u32 rem = state.mutation_count % iter_cnt;
		if (rem) {
			cmd.begin();
			for (u32 i = 0; i < rem; i++) {
				mutate(cnt++);
			}
			rg::run(cmd.handle);
			rg::submit(cmd);
		}
	}
	// Compositions
	rg::add_compute(CSTR("Composition"),
					{.shader = vk::Shader(CSTR("src/shaders/integrators/vcmmlt/composite.comp")),
					 .dims = {(u32)lm::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
		.push_constants(&state.pc)
		.bind({integrator->output_tex, integrator->lumen_scene->scene_desc_buffer});
}

bool gui(Integrator* integrator) {
	VCMMLT& state = integrator->vcmmlt;
	// bool result = false;
	// result |= ImGui::Checkbox("Enable Light-first ordering(default = eye)", &config.light_first);
	// if (config.light_first) {
	//	result |= ImGui::Checkbox("Enable VM", &config.enable_vm);
	// }
	// return result;
	return false;
}

bool update(Integrator* integrator) {
	VCMMLT& state = integrator->vcmmlt;
	integrator->frame_num++;
	bool updated = integrator->updated;
	if (updated) {
		integrator->frame_num = 0;
	}
	return updated;
}

void destroy(Integrator* integrator, bool resize) {
	VCMMLT& state = integrator->vcmmlt;
	(void)resize;

	vk::Buffer** buffers[] = {
		&state.bootstrap_buffer,	&state.cdf_buffer,			&state.cdf_sum_buffer,
		&state.seeds_buffer,		&state.mlt_samplers_buffer, &state.light_primary_samples_buffer,
		&state.mlt_col_buffer,		&state.chain_stats_buffer,	&state.splat_buffer,
		&state.past_splat_buffer,	&state.light_path_buffer,	&state.light_path_cnt_buffer,
		&state.tmp_col_buffer,		&state.photon_buffer,		&state.mlt_atomicsum_buffer,
		&state.mlt_residual_buffer, &state.counter_buffer};
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

}  // namespace vcmmlt
