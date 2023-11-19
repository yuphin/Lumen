#include "LumenPCH.h"
#include "SMLT.h"

void SMLT::init() {
	Integrator::init();
	mutations_per_pixel = config->mutations_per_pixel;
	num_mlt_threads = config->num_mlt_threads;
	num_bootstrap_samples = config->num_bootstrap_samples;
	mutation_count = int(instance->width * instance->height * mutations_per_pixel / float(num_mlt_threads));
	light_path_rand_count = 6 + 2 * config->path_length;
	cam_path_rand_count = 3 + 6 * config->path_length;

	bootstrap_buffer.create(&instance->vkb.ctx,
							VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
							num_bootstrap_samples * sizeof(BootstrapSample));

	cdf_buffer.create(&instance->vkb.ctx,
					  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
						  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, num_bootstrap_samples * 4);

	bootstrap_cpu.create(&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						 VK_SHARING_MODE_EXCLUSIVE, num_bootstrap_samples * sizeof(BootstrapSample));

	cdf_cpu.create(&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				   VK_SHARING_MODE_EXCLUSIVE, num_bootstrap_samples * 4);

	cdf_sum_buffer.create(&instance->vkb.ctx,
						  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, sizeof(float));

	seeds_buffer.create(&instance->vkb.ctx,
						VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
						num_mlt_threads * sizeof(SeedData));

	light_primary_samples_buffer.create(&instance->vkb.ctx,
										VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
											VK_BUFFER_USAGE_TRANSFER_DST_BIT,
										VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
										num_mlt_threads * light_path_rand_count * sizeof(PrimarySample));

	cam_primary_samples_buffer.create(&instance->vkb.ctx,
									  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
										  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
									  num_mlt_threads * cam_path_rand_count * sizeof(PrimarySample));

	mlt_samplers_buffer.create(&instance->vkb.ctx,
							   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
							   num_mlt_threads * sizeof(MLTSampler));

	mlt_col_buffer.create(&instance->vkb.ctx,
						  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
						  instance->width * instance->height * 3 * sizeof(float));

	chain_stats_buffer.create(&instance->vkb.ctx,
							  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
							  num_mlt_threads * sizeof(ChainData));

	splat_buffer.create(&instance->vkb.ctx,
						VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
						num_mlt_threads * (config->path_length * (config->path_length + 1)) * sizeof(Splat));

	past_splat_buffer.create(&instance->vkb.ctx,
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
							 num_mlt_threads * (config->path_length * (config->path_length + 1)) * sizeof(Splat));

	auto path_size = std::max(num_mlt_threads, num_bootstrap_samples);
	light_path_buffer.create(&instance->vkb.ctx,
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
							 path_size * (config->path_length + 1) * sizeof(VCMVertex));

	connected_lights_buffer.create(
		&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, path_size * sizeof(uint32_t));

	tmp_seeds_buffer.create(
		&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, path_size * sizeof(SeedData));

	light_path_cnt_buffer.create(&instance->vkb.ctx,
								 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
								 path_size * sizeof(float));

	light_splats_buffer.create(&instance->vkb.ctx,
							   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
							   path_size * (config->path_length * (config->path_length + 1)) * sizeof(Splat));

	light_splat_cnts_buffer.create(&instance->vkb.ctx,
								   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
								   path_size * sizeof(float));
	// ---- //
	tmp_lum_buffer.create(
		&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, num_bootstrap_samples * sizeof(float));

	prob_carryover_buffer.create(
		&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, num_mlt_threads * sizeof(uint32_t));

	int size = 0;
	int arr_size = num_bootstrap_samples;
	do {
		int num_blocks = std::max(1, (int)ceil(arr_size / (2.0f * 1024)));
		if (num_blocks > 1) {
			size++;
		}
		arr_size = num_blocks;
	} while (arr_size > 1);
	block_sums.resize(size);
	int i = 0;
	arr_size = num_bootstrap_samples;
	do {
		int num_blocks = std::max(1, (int)ceil(arr_size / (2.0f * 1024)));
		if (num_blocks > 1) {
			block_sums[i++].create(&instance->vkb.ctx,
								   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
								   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, num_blocks * 4);
		}
		arr_size = num_blocks;
	} while (arr_size > 1);

	SceneDesc desc;
	desc.vertex_addr = vertex_buffer.get_device_address();
	desc.index_addr = index_buffer.get_device_address();
	desc.normal_addr = normal_buffer.get_device_address();
	desc.uv_addr = uv_buffer.get_device_address();
	desc.material_addr = materials_buffer.get_device_address();
	desc.prim_info_addr = prim_lookup_buffer.get_device_address();
	// SMLT
	desc.bootstrap_addr = bootstrap_buffer.get_device_address();
	desc.cdf_addr = cdf_buffer.get_device_address();
	desc.cdf_sum_addr = cdf_sum_buffer.get_device_address();
	desc.seeds_addr = seeds_buffer.get_device_address();
	desc.light_primary_samples_addr = light_primary_samples_buffer.get_device_address();
	desc.cam_primary_samples_addr = cam_primary_samples_buffer.get_device_address();
	desc.mlt_samplers_addr = mlt_samplers_buffer.get_device_address();
	desc.mlt_col_addr = mlt_col_buffer.get_device_address();
	desc.chain_stats_addr = chain_stats_buffer.get_device_address();
	desc.splat_addr = splat_buffer.get_device_address();
	desc.past_splat_addr = past_splat_buffer.get_device_address();
	desc.vcm_vertices_addr = light_path_buffer.get_device_address();
	desc.connected_lights_addr = connected_lights_buffer.get_device_address();
	desc.tmp_seeds_addr = tmp_seeds_buffer.get_device_address();
	desc.path_cnt_addr = light_path_cnt_buffer.get_device_address();
	desc.tmp_lum_addr = tmp_lum_buffer.get_device_address();
	desc.prob_carryover_addr = prob_carryover_buffer.get_device_address();
	desc.light_splats_addr = light_splats_buffer.get_device_address();
	desc.light_splat_cnts_addr = light_splat_cnts_buffer.get_device_address();

	assert(instance->vkb.rg->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, &prim_lookup_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, bootstrap_addr, &bootstrap_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cdf_addr, &cdf_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cdf_sum_addr, &cdf_sum_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, seeds_addr, &seeds_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_primary_samples_addr, &light_primary_samples_buffer,
								 instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cam_primary_samples_addr, &cam_primary_samples_buffer,
								 instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_samplers_addr, &mlt_samplers_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_col_addr, &mlt_col_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, chain_stats_addr, &chain_stats_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, splat_addr, &splat_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, past_splat_addr, &past_splat_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, vcm_vertices_addr, &light_path_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, connected_lights_addr, &connected_lights_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, tmp_seeds_addr, &tmp_seeds_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, path_cnt_addr, &light_path_cnt_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, tmp_lum_addr, &tmp_lum_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prob_carryover_addr, &prob_carryover_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_splats_addr, &light_splats_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_splat_cnts_addr, &light_splat_cnts_buffer, instance->vkb.rg);

	scene_desc_buffer.create(
		&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneDesc), &desc, true);

	pc_ray.total_light_area = 0;
	pc_ray.frame_num = 0;
	frameNUM = 0;
	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;
	pc_ray.mutations_per_pixel = mutations_per_pixel;
	pc_ray.use_vc = 1;
	pc_ray.use_vm = 0;
}

void SMLT::render() {
	const float ppm_base_radius = 0.25f;
	CommandBuffer cmd(&instance->vkb.ctx, /*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VkClearValue clear_color = {0.25f, 0.25f, 0.25f, 1.0f};
	VkClearValue clear_depth = {1.0f, 0};
	VkViewport viewport = vk::viewport((float)instance->width, (float)instance->height, 0.0f, 1.0f);
	VkClearValue clear_values[] = {clear_color, clear_depth};
	pc_ray.num_lights = int(lights.size());
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	// SMLT related constants
	pc_ray.light_rand_count = light_path_rand_count;
	pc_ray.cam_rand_count = cam_path_rand_count;
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.num_bootstrap_samples = num_bootstrap_samples;
	pc_ray.total_light_area = total_light_area;
	pc_ray.light_triangle_count = total_light_triangle_cnt;

	const std::initializer_list<ResourceBinding> rt_bindings = {
		output_tex,
		scene_ubo_buffer,
		scene_desc_buffer,
	};

	// Start bootstrap sampling
	{
		// Light
		instance->vkb.rg
			->add_rt("SMLT - Bootstrap Sampling - Light",
					 {.shaders = {{"src/shaders/integrators/smlt/smlt_seed_light.rgen"},
								  {"src/shaders/ray.rmiss"},
								  {"src/shaders/ray_shadow.rmiss"},
								  {"src/shaders/ray.rchit"},
								  {"src/shaders/ray.rahit"}},
					  .specialization_data = {1},
					  .dims = {(uint32_t)num_bootstrap_samples},
					  .accel = instance->vkb.tlas.accel})
			.push_constants(&pc_ray)
			.bind(rt_bindings)
			.bind(mesh_lights_buffer)
			.bind_texture_array(scene_textures)
			.bind_tlas(instance->vkb.tlas);
		// Eye
		instance->vkb.rg
			->add_rt("SMLT - Bootstrap Sampling - Eye",
					 {.shaders = {{"src/shaders/integrators/smlt/smlt_seed_eye.rgen"},
								  {"src/shaders/ray.rmiss"},
								  {"src/shaders/ray_shadow.rmiss"},
								  {"src/shaders/ray.rchit"},
								  {"src/shaders/ray.rahit"}},
					  .specialization_data = {1},
					  .dims = {(uint32_t)num_bootstrap_samples},
					  .accel = instance->vkb.tlas.accel})
			.push_constants(&pc_ray)
			.bind(rt_bindings)
			.bind(mesh_lights_buffer)
			.bind_texture_array(scene_textures)
			.bind_tlas(instance->vkb.tlas);
	}
	int counter = 0;
	prefix_scan(0, config->num_bootstrap_samples, counter, instance->vkb.rg.get());
	// Calculate CDF
	instance->vkb.rg
		->add_compute("Calculate CDF", {.shader = Shader("src/shaders/integrators/pssmlt/calc_cdf.comp"),
										.dims = {(uint32_t)std::ceil(num_bootstrap_samples / float(1024.0f)), 1, 1}})
		.push_constants(&pc_ray)
		.bind(scene_desc_buffer);
#if 0
	// Debugging code
	bootstrap_buffer.copy(bootstrap_cpu, cmd.handle);
	cdf_buffer.copy(cdf_cpu, cmd.handle);
	cmd.submit();
	std::vector<BootstrapSample> samples;
	samples.assign((BootstrapSample*)bootstrap_cpu.data,
				   (BootstrapSample*)bootstrap_cpu.data + num_bootstrap_samples);
	std::vector<float> cdf1(num_bootstrap_samples, 0);
	std::vector<float> cdf2(num_bootstrap_samples, 0);

	cdf1[0] = 0;
	for (int i = 1; i < num_bootstrap_samples; i++) {
		cdf1[i] = cdf1[i - 1] + samples[i - 1].lum / num_bootstrap_samples;
	}
	float sum = cdf1[num_bootstrap_samples - 1];
	for (int i = 1; i < num_bootstrap_samples; i++) {
		cdf1[i] *= 1. / sum;
	}
	cdf2.assign((float*)cdf_cpu.data,
				(float*)cdf_cpu.data + num_bootstrap_samples);
	float EPS = 1e-3;
	for (int i = 0; i < num_bootstrap_samples; i++) {
		float val1 = cdf1[i];
		float val2 = cdf2[i];
		float diff = abs(val1 - val2);
		if (diff > EPS) {
			assert(false);
		}

	}
	sum = cdf1[num_bootstrap_samples - 1];
	float sum2 = cdf2[num_bootstrap_samples - 1];
	return;
#endif
	// Select seeds
	instance->vkb.rg
		->add_compute("Select Seeds", {.shader = Shader("src/shaders/integrators/pssmlt/select_seeds.comp"),
									   .dims = {(uint32_t)std::ceil(num_mlt_threads / float(1024.0f)), 1, 1}})
		.push_constants(&pc_ray)
		.bind(scene_desc_buffer);
	// Fill in the samplers for mutations
	{
		// Light
		instance->vkb.rg
			->add_rt("SMLT - Preprocess - Light",
					 {.shaders = {{"src/shaders/integrators/smlt/smlt_preprocess_light.rgen"},
								  {"src/shaders/ray.rmiss"},
								  {"src/shaders/ray_shadow.rmiss"},
								  {"src/shaders/ray.rchit"},
								  {"src/shaders/ray.rahit"}},
					  .dims = {(uint32_t)num_mlt_threads},
					  .accel = instance->vkb.tlas.accel})
			.push_constants(&pc_ray)
			.zero(mlt_samplers_buffer)
			.bind(rt_bindings)
			.bind(mesh_lights_buffer)
			.bind_texture_array(scene_textures)
			.bind_tlas(instance->vkb.tlas);
		// Eye
		instance->vkb.rg
			->add_rt("SMLT - Preprocess - Eye", {.shaders = {{"src/shaders/integrators/smlt/smlt_preprocess_eye.rgen"},
															 {"src/shaders/ray.rmiss"},
															 {"src/shaders/ray_shadow.rmiss"},
															 {"src/shaders/ray.rchit"},
															 {"src/shaders/ray.rahit"}},
												 .dims = {(uint32_t)num_mlt_threads},
												 .accel = instance->vkb.tlas.accel})
			.push_constants(&pc_ray)
			.zero(mlt_samplers_buffer)
			.bind(rt_bindings)
			.bind(mesh_lights_buffer)
			.bind_texture_array(scene_textures)
			.bind_tlas(instance->vkb.tlas);
	}
	instance->vkb.rg->run_and_submit(cmd);
	// Start mutations
	{
		auto mutate = [&](uint32_t i) {
			pc_ray.random_num = rand() % UINT_MAX;
			pc_ray.mutation_counter = i;
			// Light
			instance->vkb.rg
				->add_rt("PSSMLT - Mutate - Light",
						 {.shaders = {{"src/shaders/integrators/smlt/smlt_mutate_light.rgen"},
									  {"src/shaders/ray.rmiss"},
									  {"src/shaders/ray_shadow.rmiss"},
									  {"src/shaders/ray.rchit"},
									  {"src/shaders/ray.rahit"}},
						  .dims = {(uint32_t)num_mlt_threads},
						  .accel = instance->vkb.tlas.accel})
				.push_constants(&pc_ray)
				.bind(rt_bindings)
				.bind(mesh_lights_buffer)
				.bind_texture_array(scene_textures)
				.bind_tlas(instance->vkb.tlas);
			// Eye
			instance->vkb.rg
				->add_rt("PSSMLT - Mutate - Eye", {.shaders = {{"src/shaders/integrators/smlt/smlt_mutate_eye.rgen"},
															   {"src/shaders/ray.rmiss"},
															   {"src/shaders/ray_shadow.rmiss"},
															   {"src/shaders/ray.rchit"},
															   {"src/shaders/ray.rahit"}},
												   .dims = {(uint32_t)num_mlt_threads},
												   .accel = instance->vkb.tlas.accel})
				.push_constants(&pc_ray)
				.bind(rt_bindings)
				.bind(mesh_lights_buffer)
				.bind_texture_array(scene_textures)
				.bind_tlas(instance->vkb.tlas);
		};
		const uint32_t iter_cnt = 100;
		const uint32_t freq = mutation_count / iter_cnt;
		int iter = 0;
		for (uint32_t f = 0; f < freq; f++) {
			cmd.begin();
			for (int i = 0; i < iter_cnt; i++) {
				mutate(i);
			}
			iter += 100;
			instance->vkb.rg->run(cmd.handle);
			LUMEN_TRACE("{} / {}", iter, mutation_count);
			instance->vkb.rg->submit(cmd);
		}
		const uint32_t rem = mutation_count % iter_cnt;
		if (rem) {
			cmd.begin();
			for (uint32_t i = 0; i < rem; i++) {
				mutate(i);
			}
			instance->vkb.rg->run(cmd.handle);
			instance->vkb.rg->submit(cmd);
		}
	}
	// Compositions
	instance->vkb.rg
		->add_compute("Composition",
					  {.shader = Shader("src/shaders/integrators/pssmlt/composite.comp"),
					   .dims = {(uint32_t)std::ceil(instance->width * instance->height / float(1024.0f)), 1, 1}})
		.push_constants(&pc_ray)
		.bind({output_tex, scene_desc_buffer});
}

bool SMLT::update() {
	pc_ray.frame_num++;
	frameNUM++;
	bool updated = Integrator::update();
	if (updated) {
		pc_ray.frame_num = 0;
		frameNUM = 0;
	}
	return updated;
}

void SMLT::prefix_scan(int level, int num_elems, int& counter, RenderGraph* rg) {
	const bool scan_sums = level > 0;
	int num_wgs = std::max(1, (int)ceil(num_elems / (2 * 1024.0f)));
	int num_grids = num_wgs - int((num_elems % 2048) != 0);
	pc_compute.num_elems = num_elems;
	auto scan = [&](int num_wgs, int idx) {
		++counter;
		rg->add_compute("PrefixScan - Scan", {.shader = Shader("src/shaders/integrators/pssmlt/prefix_scan.comp"),
											  .dims = {(uint32_t)num_wgs, 1, 1}})
			.push_constants(&pc_compute)
			.bind(scene_desc_buffer);
	};
	auto uniform_add = [&](int num_wgs, int output_idx) {
		++counter;
		rg->add_compute(
			  "PrefixScan - Uniform Add",
			  {.shader = Shader("src/shaders/integrators/pssmlt/uniform_add.comp"), .dims = {(uint32_t)num_wgs, 1, 1}})
			.push_constants(&pc_compute)
			.bind(scene_desc_buffer);
	};
	if (num_wgs > 1) {
		pc_compute.base_idx = 0;
		pc_compute.block_idx = 0;
		pc_compute.n = 2 * 1024;
		pc_compute.store_sum = 1;
		pc_compute.scan_sums = int(scan_sums);
		pc_compute.block_sum_addr = block_sums[level].get_device_address();
		scan(num_grids, level);
		int rem = num_elems % (2 * 1024);
		if (rem) {
			pc_compute.base_idx = num_elems - rem;
			pc_compute.block_idx = num_wgs - 1;
			pc_compute.n = rem;
			scan(1, level);
		}
		prefix_scan(level + 1, num_wgs, counter, rg);
		pc_compute.base_idx = 0;
		pc_compute.block_idx = 0;
		pc_compute.n = num_elems - rem;
		pc_compute.store_sum = 1;
		pc_compute.scan_sums = int(scan_sums);
		pc_compute.block_sum_addr = block_sums[level].get_device_address();
		if (scan_sums) {
			pc_compute.out_addr = block_sums[level - 1].get_device_address();
		}
		uniform_add(num_grids, level - 1);
		if (rem) {
			pc_compute.base_idx = num_elems - rem;
			pc_compute.block_idx = num_wgs - 1;
			pc_compute.n = rem;
			uniform_add(1, level - 1);
		}
	} else {
		int rem = num_elems % 2048;
		pc_compute.n = rem == 0 ? 2048 : rem;
		pc_compute.base_idx = 0;
		pc_compute.block_idx = 0;
		pc_compute.store_sum = 0;
		pc_compute.scan_sums = bool(scan_sums);
		if (scan_sums) {
			pc_compute.block_sum_addr = block_sums[level - 1].get_device_address();
		}
		scan(num_wgs, level - 1);
	}
}

void SMLT::destroy() {
	const auto device = instance->vkb.ctx.device;
	Integrator::destroy();
	std::vector<Buffer*> buffer_list = {&bootstrap_buffer,
										&cdf_buffer,
										&cdf_sum_buffer,
										&seeds_buffer,
										&mlt_samplers_buffer,
										&light_primary_samples_buffer,
										&cam_primary_samples_buffer,
										&mlt_col_buffer,
										&chain_stats_buffer,
										&splat_buffer,
										&past_splat_buffer,
										&light_path_buffer,
										&connected_lights_buffer,
										&tmp_seeds_buffer,
										&tmp_lum_buffer,
										&prob_carryover_buffer,
										&light_path_cnt_buffer};
	for (auto b : buffer_list) {
		b->destroy();
	}
	for (auto& b : block_sums) {
		b.destroy();
	}
	if (bootstrap_cpu.size) {
		bootstrap_cpu.destroy();
	}
	if (cdf_cpu.size) {
		cdf_cpu.destroy();
	}
}
