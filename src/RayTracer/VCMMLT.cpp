#include "Framework/RenderGraph.h"
#include "VCMMLT.h"
static bool use_vm = false;
static f32 vcm_radius_factor = 0.025f;
static bool light_first = false;
void VCMMLT::init() {
	Integrator::init();

	const VCMMLTConfig& config = lumen_scene->config.settings.vcmmlt;
	u32 path_length = lumen_scene->config.common.path_length;
	mutation_count = i32(Window::width() * Window::height() * config.mutations_per_pixel / f32(config.num_mlt_threads));
	light_path_rand_count = glm::max(7 + 3 * path_length, 3 + 7 * path_length);

	// MLTVCM buffers
	bootstrap_buffer =
		prm::get_buffer({.name = "Bootstrap Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_bootstrap_samples * sizeof(BootstrapSample)});

	cdf_buffer =
		prm::get_buffer({.name = "CDF Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = VkDeviceSize(config.num_bootstrap_samples * 4)});

	cdf_sum_buffer =
		prm::get_buffer({.name = "CDF Sum Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(f32)});

	seeds_buffer =
		prm::get_buffer({.name = "Seeds Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * sizeof(VCMMLTSeedData)});

	light_primary_samples_buffer =
		prm::get_buffer({.name = "Light Primary Samples Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * light_path_rand_count * sizeof(PrimarySample) * 2});

	mlt_samplers_buffer =
		prm::get_buffer({.name = "MLT Samplers Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * sizeof(VCMMLTSampler) * 2});

	mlt_col_buffer =
		prm::get_buffer({.name = "MLT Col Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * 3 * sizeof(f32)});

	chain_stats_buffer =
		prm::get_buffer({.name = "Chain Stats Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = 2 * sizeof(ChainData)});

	splat_buffer =
		prm::get_buffer({.name = "Splat Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * (path_length * (path_length + 1)) * sizeof(Splat) * 2});

	past_splat_buffer =
		prm::get_buffer({.name = "Past Splat Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * (path_length * (path_length + 1)) * sizeof(Splat) * 2});

	light_path_buffer =
		prm::get_buffer({.name = "Light Path Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * (path_length + 1) * sizeof(VCMVertex)});

	light_path_cnt_buffer =
		prm::get_buffer({.name = "Light Path Cnt Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(f32)});

	tmp_col_buffer =
		prm::get_buffer({.name = "Tmp Col Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = Window::width() * Window::height() * sizeof(f32) * 3});

	photon_buffer =
		prm::get_buffer({.name = "Photon Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = 10 * Window::width() * Window::height() * sizeof(VCMPhotonHash)});

	mlt_atomicsum_buffer =
		prm::get_buffer({.name = "MLT Atomic Sum Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * sizeof(SumData) * 2});

	mlt_residual_buffer =
		prm::get_buffer({.name = "MLT Residual Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = config.num_mlt_threads * sizeof(SumData)});

	counter_buffer =
		prm::get_buffer({.name = "Counter Buffer",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(i32)});

	i32 size = 0;
	i32 arr_size = config.num_bootstrap_samples;
	do {
		i32 num_blocks = glm::max(1, (i32)ceil(arr_size / (2.0f * 1024)));
		if (num_blocks > 1) {
			size++;
		}
		arr_size = num_blocks;
	} while (arr_size > 1);
	block_sums.resize(size);
	i32 i = 0;
	arr_size = config.num_bootstrap_samples;
	do {
		i32 num_blocks = glm::max(1, (i32)ceil(arr_size / (2.0f * 1024)));
		if (num_blocks > 1) {
			block_sums[i++] = prm::get_buffer(
				{.name = "Block Sum Buffer #" + std::to_string(i),
				 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				 .memory_type = vk::BUFFER_TYPE_GPU,
				 .size = VkDeviceSize(num_blocks * 4)});
		}
		arr_size = num_blocks;
	} while (arr_size > 1);

	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer->device_address();

	desc.material_addr = lumen_scene->materials_buffer->device_address();
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer->device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer->device_address();
	// VCMMLT
	desc.bootstrap_addr = bootstrap_buffer->device_address();
	desc.cdf_addr = cdf_buffer->device_address();
	desc.cdf_sum_addr = cdf_sum_buffer->device_address();
	desc.seeds_addr = seeds_buffer->device_address();
	desc.light_primary_samples_addr = light_primary_samples_buffer->device_address();
	desc.mlt_samplers_addr = mlt_samplers_buffer->device_address();
	desc.mlt_col_addr = mlt_col_buffer->device_address();
	desc.chain_stats_addr = chain_stats_buffer->device_address();
	desc.splat_addr = splat_buffer->device_address();
	desc.past_splat_addr = past_splat_buffer->device_address();

	desc.vcm_vertices_addr = light_path_buffer->device_address();
	desc.path_cnt_addr = light_path_cnt_buffer->device_address();

	desc.color_storage_addr = tmp_col_buffer->device_address();
	desc.photon_addr = photon_buffer->device_address();

	desc.mlt_atomicsum_addr = mlt_atomicsum_buffer->device_address();
	desc.residual_addr = mlt_residual_buffer->device_address();
	desc.counter_addr = counter_buffer->device_address();

	assert(vk::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, lumen_scene->prim_lookup_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, bootstrap_addr, bootstrap_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cdf_addr, cdf_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cdf_sum_addr, cdf_sum_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, seeds_addr, seeds_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_primary_samples_addr, light_primary_samples_buffer,
								 vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_samplers_addr, mlt_samplers_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_col_addr, mlt_col_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, chain_stats_addr, chain_stats_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, splat_addr, splat_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, past_splat_addr, past_splat_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, vcm_vertices_addr, light_path_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, path_cnt_addr, light_path_cnt_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, tmp_col_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, photon_addr, photon_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_atomicsum_addr, mlt_atomicsum_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, residual_addr, mlt_residual_buffer, vk::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, counter_addr, counter_buffer, vk::render_graph());

	lumen_scene->scene_desc_buffer =
		prm::get_buffer({.name = "Scene Desc",
						 .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						 .memory_type = vk::BUFFER_TYPE_GPU,
						 .size = sizeof(SceneDesc),
						 .data = &desc});
	pc_ray.total_light_area = 0;

	frame_num = 0;

	pc_ray.mutations_per_pixel = config.mutations_per_pixel;
	pc_ray.num_mlt_threads = config.num_mlt_threads;
}

void VCMMLT::render() {
	LUMEN_TRACE("Rendering sample %d...", sample_cnt++);
	vk::CommandBuffer cmd(/*start*/ true);
	const VCMMLTConfig& config = lumen_scene->config.settings.vcmmlt;
	pc_ray.size_x = Window::width();
	pc_ray.size_y = Window::height();
	pc_ray.num_lights = i32(lumen_scene->gpu_lights.size);
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = lumen_scene->config.common.path_length;
	pc_ray.sky_col = lumen_scene->config.common.sky_col;
	pc_ray.frame_num = frame_num;
	// VCMMLT related constants
	pc_ray.use_vm = use_vm;
	pc_ray.light_rand_count = light_path_rand_count;
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.num_bootstrap_samples = config.num_bootstrap_samples;
	pc_ray.radius = lumen_scene->dimensions.radius * vcm_radius_factor / 100.f;
	pc_ray.radius /= (f32)pow((double)pc_ray.frame_num + 1, 0.5 * (1 - 2.0 / 3));
	pc_ray.min_bounds = lumen_scene->dimensions.min;
	pc_ray.max_bounds = lumen_scene->dimensions.max;
	const glm::vec3 diam = pc_ray.max_bounds - pc_ray.min_bounds;
	const f32 max_comp = glm::max(diam.x, glm::max(diam.y, diam.z));
	const i32 base_grid_res = i32(max_comp / pc_ray.radius);
	pc_ray.grid_res = glm::max(ivec3(diam * f32(base_grid_res) / max_comp), ivec3(1));
	pc_ray.total_light_area = lumen_scene->total_light_area;
	pc_ray.light_triangle_count = lumen_scene->total_light_triangle_cnt;

	lm::RenderGraph* rg = vk::render_graph();
	auto get_pipeline_postfix = [&](const std::vector<u32>& spec_consts) {
		std::string res = "-";
		if (spec_consts[0] == 1) {
			res += "SEED";
		}
		if (spec_consts[1] == 1) {
			res += "&LIGHT_FIRST";
		}
		return res;
	};
	auto op_reduce = [&](const lm::String& op_name, const lm::String& op_shader_name, const lm::String& reduce_name,
						 const lm::String& reduce_shader_name, const std::vector<u32> spec_data) {
		u32 num_wgs = u32((config.num_mlt_threads + 1023) / 1024);
		rg->add_compute(
			  op_name,
			  {.shader = vk::Shader(op_shader_name), .specialization_data = spec_data, .dims = {num_wgs, 1, 1}})
			.push_constants(&pc_ray)
			.bind(lumen_scene->scene_desc_buffer)
			.zero({mlt_residual_buffer, counter_buffer});
		while (num_wgs != 1) {
			rg->add_compute(
				  reduce_name,
				  {.shader = vk::Shader(reduce_shader_name), .specialization_data = spec_data, .dims = {num_wgs, 1, 1}})
				.push_constants(&pc_ray)
				.bind(lumen_scene->scene_desc_buffer);
			num_wgs = (u32)(num_wgs + 1023) / 1024;
		}
	};
	auto sum_up_chain_data = [&] {
		op_reduce("OpReduce: Sum0", "src/shaders/integrators/vcmmlt/sum.comp", "OpReduce: Reduce Sum0",
				  "src/shaders/integrators/vcmmlt/reduce_sum.comp", {0});
		op_reduce("OpReduce: Sum1", "src/shaders/integrators/vcmmlt/sum.comp", "OpReduce: Reduce Sum1",
				  "src/shaders/integrators/vcmmlt/reduce_sum.comp", {1});
	};
	std::initializer_list<lm::ResourceBinding> rt_bindings = {
		output_tex,
		scene_ubo_buffer,
		lumen_scene->scene_desc_buffer,
	};
	std::vector<u32> spec_consts;
	if (!light_first) {
		spec_consts = {1, 0};
	} else {
		spec_consts = {1, 1};
	}
	std::string pipeline_postfix = get_pipeline_postfix(spec_consts);
	// Shoot rays
	std::string pipeline_name = "VCMMLT - Trace " + pipeline_postfix;
	rg->add_rt(lm::str_from_cpp_str(pipeline_name),
			   {
				   .shaders = {{"src/shaders/integrators/vcmmlt/vcmmlt_eye.rgen"},
							   {"src/shaders/ray.rmiss"},
							   {"src/shaders/ray_shadow.rmiss"},
							   {"src/shaders/ray.rchit"},
							   {"src/shaders/ray.rahit"}},
				   .specialization_data = spec_consts,
				   .dims = {Window::width() * Window::height()},
			   })
		.push_constants(&pc_ray)
		.zero({chain_stats_buffer, mlt_atomicsum_buffer})
		.zero(photon_buffer, use_vm)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(tlas);
	// Start bootstrap sampling
	pipeline_name = "VCMMLT - Bootstrap " + pipeline_postfix;
	rg->add_rt(lm::str_from_cpp_str(pipeline_name),
			   {
				   .shaders = {{"src/shaders/integrators/vcmmlt/vcmmlt_seed.rgen"},
							   {"src/shaders/ray.rmiss"},
							   {"src/shaders/ray_shadow.rmiss"},
							   {"src/shaders/ray.rchit"},
							   {"src/shaders/ray.rahit"}},
				   .specialization_data = spec_consts,
				   .dims = {(u32)config.num_bootstrap_samples},
			   })
		.push_constants(&pc_ray)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(tlas);
	i32 counter = 0;
	prefix_scan(0, config.num_bootstrap_samples, counter, rg);
	// Calculate CDF
	rg->add_compute("Calculate CDF", {.shader = vk::Shader("src/shaders/integrators/pssmlt/calc_cdf.comp"),
									  .dims = {(u32)std::ceil(config.num_bootstrap_samples / f32(1024.0f)), 1, 1}})
		.push_constants(&pc_ray)
		.bind(lumen_scene->scene_desc_buffer);
	// Select seeds
	rg->add_compute("Select Seeds", {.shader = vk::Shader("src/shaders/integrators/vcmmlt/select_seeds.comp"),
									 .dims = {(u32)std::ceil(config.num_mlt_threads / f32(1024.0f)), 1, 1}})
		.push_constants(&pc_ray)
		.bind(lumen_scene->scene_desc_buffer);
	// Fill in the samplers for mutations
	{
		// Fill
		std::string pipeline_name = "VCMMLT - Preprocess " + pipeline_postfix;
		rg->add_rt(lm::str_from_cpp_str(pipeline_name),
				   {
					   .shaders = {{"src/shaders/integrators/vcmmlt/vcmmlt_preprocess.rgen"},
								   {"src/shaders/ray.rmiss"},
								   {"src/shaders/ray_shadow.rmiss"},
								   {"src/shaders/ray.rchit"},
								   {"src/shaders/ray.rahit"}},
					   .specialization_data = spec_consts,
					   .dims = {(u32)config.num_mlt_threads},
				   })
			.push_constants(&pc_ray)
			.bind(rt_bindings)
			.bind(lumen_scene->mesh_lights_buffer)
			.bind_texture_array(lumen_scene->scene_textures)
			.bind_tlas(tlas);
		// Sum up chain stats
		sum_up_chain_data();
	}
	// Calculate normalization factor
	rg->add_compute("Calculate Normalization",
					{.shader = vk::Shader("src/shaders/integrators/vcmmlt/normalize.comp"), .dims = {1, 1, 1}})
		.push_constants(&pc_ray)
		.bind(lumen_scene->scene_desc_buffer);
	rg->run_and_submit(cmd);
	// Start mutations
	{
		std::string pipeline_name = "VCMMLT - Mutate " + pipeline_postfix;
		auto mutate = [&](u32 i) {
			pc_ray.random_num = rand() % UINT_MAX;
			pc_ray.mutation_counter = i;
			// Mutate
			rg->add_rt(lm::str_from_cpp_str(pipeline_name),
					   {
						   .shaders = {{"src/shaders/integrators/vcmmlt/vcmmlt_mutate.rgen"},
									   {"src/shaders/ray.rmiss"},
									   {"src/shaders/ray_shadow.rmiss"},
									   {"src/shaders/ray.rchit"},
									   {"src/shaders/ray.rahit"}},
						   .dims = {(u32)config.num_mlt_threads},
					   })
				.push_constants(&pc_ray)
				.zero(mlt_atomicsum_buffer)
				.bind(rt_bindings)
				.bind(lumen_scene->mesh_lights_buffer)
				.bind_texture_array(lumen_scene->scene_textures)
				.bind_tlas(tlas);
			sum_up_chain_data();
			// Normalization
			rg->add_compute("Calculate Normalization",
							{.shader = vk::Shader("src/shaders/integrators/vcmmlt/normalize.comp"), .dims = {1, 1, 1}})
				.push_constants(&pc_ray)
				.bind(lumen_scene->scene_desc_buffer);
		};
		const u32 iter_cnt = 100;
		const u32 freq = mutation_count / iter_cnt;
		u32 cnt = 0;
		for (u32 f = 0; f < freq; f++) {
			LUMEN_TRACE("Mutation: %d / %d", cnt, mutation_count);
			cmd.begin();
			for (i32 i = 0; i < iter_cnt; i++) {
				mutate(cnt++);
			}
			rg->run(cmd.handle);
			rg->submit(cmd);
		}
		const u32 rem = mutation_count % iter_cnt;
		if (rem) {
			cmd.begin();
			for (u32 i = 0; i < rem; i++) {
				mutate(cnt++);
			}
			rg->run(cmd.handle);
			rg->submit(cmd);
		}
	}
	// Compositions
	rg->add_compute("Composition", {.shader = vk::Shader("src/shaders/integrators/vcmmlt/composite.comp"),
									.dims = {(u32)glm::ceil(Window::width() * Window::height() / f32(1024.0f)), 1, 1}})
		.push_constants(&pc_ray)
		.bind({output_tex, lumen_scene->scene_desc_buffer});
}

bool VCMMLT::gui() {
	// bool result = false;
	// result |= ImGui::Checkbox("Enable Light-first ordering(default = eye)", &light_first);
	// if (light_first) {
	//	result |= ImGui::Checkbox("Enable VM", &use_vm);
	// }
	// return result;
	return false;
}

bool VCMMLT::update() {
	frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		frame_num = 0;
	}
	return updated;
}

void VCMMLT::prefix_scan(i32 level, i32 num_elems, i32& counter, lm::RenderGraph* rg) {
	const bool scan_sums = level > 0;
	i32 num_wgs = glm::max(1, (i32)ceil(num_elems / (2 * 1024.0f)));
	i32 num_grids = num_wgs - i32((num_elems % 2048) != 0);
	pc_compute.num_elems = num_elems;
	auto scan = [&](i32 num_wgs, i32 idx) {
		++counter;
		rg->add_compute("PrefixScan - Scan", {.shader = vk::Shader("src/shaders/integrators/pssmlt/prefix_scan.comp"),
											  .dims = {(u32)num_wgs, 1, 1}})
			.push_constants(&pc_compute)
			.bind(lumen_scene->scene_desc_buffer);
	};
	auto uniform_add = [&](i32 num_wgs, i32 output_idx) {
		++counter;
		rg->add_compute(
			  "PrefixScan - Uniform Add",
			  {.shader = vk::Shader("src/shaders/integrators/pssmlt/uniform_add.comp"), .dims = {(u32)num_wgs, 1, 1}})
			.push_constants(&pc_compute)
			.bind(lumen_scene->scene_desc_buffer);
	};
	if (num_wgs > 1) {
		pc_compute.base_idx = 0;
		pc_compute.block_idx = 0;
		pc_compute.n = 2 * 1024;
		pc_compute.store_sum = 1;
		pc_compute.scan_sums = i32(scan_sums);
		pc_compute.block_sum_addr = block_sums[level]->device_address();
		scan(num_grids, level);
		i32 rem = num_elems % (2 * 1024);
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
		pc_compute.scan_sums = i32(scan_sums);
		pc_compute.block_sum_addr = block_sums[level]->device_address();
		if (scan_sums) {
			pc_compute.out_addr = block_sums[level - 1]->device_address();
		}
		uniform_add(num_grids, level - 1);
		if (rem) {
			pc_compute.base_idx = num_elems - rem;
			pc_compute.block_idx = num_wgs - 1;
			pc_compute.n = rem;
			uniform_add(1, level - 1);
		}
	} else {
		i32 rem = num_elems % 2048;
		pc_compute.n = rem == 0 ? 2048 : rem;
		pc_compute.base_idx = 0;
		pc_compute.block_idx = 0;
		pc_compute.store_sum = 0;
		pc_compute.scan_sums = bool(scan_sums);
		if (scan_sums) {
			pc_compute.block_sum_addr = block_sums[level - 1]->device_address();
		}
		scan(num_wgs, level - 1);
	}
}

void VCMMLT::destroy(bool resize) {
	Integrator::destroy(resize);
	auto buffer_list = {bootstrap_buffer,	 cdf_buffer,		  cdf_sum_buffer,
						seeds_buffer,		 mlt_samplers_buffer, light_primary_samples_buffer,
						mlt_col_buffer,		 chain_stats_buffer,  splat_buffer,
						past_splat_buffer,	 light_path_buffer,	  light_path_cnt_buffer,
						tmp_col_buffer,		 photon_buffer,		  mlt_atomicsum_buffer,
						mlt_residual_buffer, counter_buffer};
	for (vk::Buffer* b : buffer_list) {
		prm::remove(b);
	}
	for (auto& b : block_sums) {
		prm::remove(b);
	}
}
