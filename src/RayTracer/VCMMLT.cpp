#include "Framework/RenderGraph.h"
#include "LumenPCH.h"
#include "VCMMLT.h"
static bool use_vm = false;
static float vcm_radius_factor = 0.025f;
static bool light_first = false;
void VCMMLT::init() {
	Integrator::init();
	mutation_count =
		int(instance->width * instance->height * config->mutations_per_pixel / float(config->num_mlt_threads));
	light_path_rand_count = std::max(7 + 3 * config->path_length, 3 + 7 * config->path_length);

	// MLTVCM buffers
	bootstrap_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							config->num_bootstrap_samples * sizeof(BootstrapSample));

	cdf_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
						  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, config->num_bootstrap_samples * 4);

	bootstrap_cpu.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						 config->num_bootstrap_samples * sizeof(BootstrapSample));

	cdf_cpu.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				   config->num_bootstrap_samples * 4);

	cdf_sum_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(float));

	seeds_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, config->num_mlt_threads * sizeof(VCMMLTSeedData));

	light_primary_samples_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
											VK_BUFFER_USAGE_TRANSFER_DST_BIT,
										VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
										config->num_mlt_threads * light_path_rand_count * sizeof(PrimarySample) * 2);

	mlt_samplers_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							   config->num_mlt_threads * sizeof(VCMMLTSampler) * 2);

	mlt_col_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, instance->width * instance->height * 3 * sizeof(float));

	chain_stats_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 2 * sizeof(ChainData));

	splat_buffer.create(

		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		config->num_mlt_threads * (config->path_length * (config->path_length + 1)) * sizeof(Splat) * 2);

	past_splat_buffer.create(

		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		config->num_mlt_threads * (config->path_length * (config->path_length + 1)) * sizeof(Splat) * 2);

	light_path_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							 instance->width * instance->height * (config->path_length + 1) * sizeof(VCMVertex));

	light_path_cnt_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								 instance->width * instance->height * sizeof(float));

	tmp_col_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, instance->width * instance->height * sizeof(float) * 3);

	photon_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						 10 * instance->width * instance->height * sizeof(VCMPhotonHash));

	mlt_atomicsum_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
									VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, config->num_mlt_threads * sizeof(SumData) * 2);

	mlt_residual_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, config->num_mlt_threads * sizeof(SumData));

	counter_buffer.create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(int));

	int size = 0;
	int arr_size = config->num_bootstrap_samples;
	do {
		int num_blocks = std::max(1, (int)ceil(arr_size / (2.0f * 1024)));
		if (num_blocks > 1) {
			size++;
		}
		arr_size = num_blocks;
	} while (arr_size > 1);
	block_sums.resize(size);
	int i = 0;
	arr_size = config->num_bootstrap_samples;
	do {
		int num_blocks = std::max(1, (int)ceil(arr_size / (2.0f * 1024)));
		if (num_blocks > 1) {
			block_sums[i++].create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
								   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, num_blocks * 4);
		}
		arr_size = num_blocks;
	} while (arr_size > 1);

	SceneDesc desc;
	desc.index_addr = lumen_scene->index_buffer.get_device_address();

	desc.material_addr = lumen_scene->materials_buffer.get_device_address();
	desc.prim_info_addr = lumen_scene->prim_lookup_buffer.get_device_address();
	desc.compact_vertices_addr = lumen_scene->compact_vertices_buffer.get_device_address();
	// VCMMLT
	desc.bootstrap_addr = bootstrap_buffer.get_device_address();
	desc.cdf_addr = cdf_buffer.get_device_address();
	desc.cdf_sum_addr = cdf_sum_buffer.get_device_address();
	desc.seeds_addr = seeds_buffer.get_device_address();
	desc.light_primary_samples_addr = light_primary_samples_buffer.get_device_address();
	desc.mlt_samplers_addr = mlt_samplers_buffer.get_device_address();
	desc.mlt_col_addr = mlt_col_buffer.get_device_address();
	desc.chain_stats_addr = chain_stats_buffer.get_device_address();
	desc.splat_addr = splat_buffer.get_device_address();
	desc.past_splat_addr = past_splat_buffer.get_device_address();

	desc.vcm_vertices_addr = light_path_buffer.get_device_address();
	desc.path_cnt_addr = light_path_cnt_buffer.get_device_address();

	desc.color_storage_addr = tmp_col_buffer.get_device_address();
	desc.photon_addr = photon_buffer.get_device_address();

	desc.mlt_atomicsum_addr = mlt_atomicsum_buffer.get_device_address();
	desc.residual_addr = mlt_residual_buffer.get_device_address();
	desc.counter_addr = counter_buffer.get_device_address();

	assert(lumen::VulkanBase::render_graph()->settings.shader_inference == true);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, &lumen_scene->prim_lookup_buffer,
								 lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, bootstrap_addr, &bootstrap_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cdf_addr, &cdf_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, cdf_sum_addr, &cdf_sum_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, seeds_addr, &seeds_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, light_primary_samples_addr, &light_primary_samples_buffer,
								 lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_samplers_addr, &mlt_samplers_buffer,
								 lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_col_addr, &mlt_col_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, chain_stats_addr, &chain_stats_buffer,
								 lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, splat_addr, &splat_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, past_splat_addr, &past_splat_buffer,
								 lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, vcm_vertices_addr, &light_path_buffer,
								 lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, path_cnt_addr, &light_path_cnt_buffer,
								 lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, color_storage_addr, &tmp_col_buffer,
								 lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, photon_addr, &photon_buffer, lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, mlt_atomicsum_addr, &mlt_atomicsum_buffer,
								 lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, residual_addr, &mlt_residual_buffer,
								 lumen::VulkanBase::render_graph());
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, counter_addr, &counter_buffer, lumen::VulkanBase::render_graph());

	lumen_scene->scene_desc_buffer.create(
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, sizeof(SceneDesc), &desc, true);
	pc_ray.total_light_area = 0;

	frame_num = 0;

	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;
	pc_ray.mutations_per_pixel = config->mutations_per_pixel;
	pc_ray.num_mlt_threads = config->num_mlt_threads;
}

void VCMMLT::render() {
	LUMEN_TRACE("Rendering sample {}...", sample_cnt++);
	const float ppm_base_radius = 0.25f;
	lumen::CommandBuffer cmd(/*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VkClearValue clear_color = {0.25f, 0.25f, 0.25f, 1.0f};
	VkClearValue clear_depth = {1.0f, 0};
	VkViewport viewport = vk::viewport((float)instance->width, (float)instance->height, 0.0f, 1.0f);
	VkClearValue clear_values[] = {clear_color, clear_depth};
	pc_ray.num_lights = int(lumen_scene->gpu_lights.size());
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.frame_num = frame_num;
	// VCMMLT related constants
	pc_ray.use_vm = use_vm;
	pc_ray.light_rand_count = light_path_rand_count;
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.num_bootstrap_samples = config->num_bootstrap_samples;
	pc_ray.radius = lumen_scene->m_dimensions.radius * vcm_radius_factor / 100.f;
	pc_ray.radius /= (float)pow((double)pc_ray.frame_num + 1, 0.5 * (1 - 2.0 / 3));
	pc_ray.min_bounds = lumen_scene->m_dimensions.min;
	pc_ray.max_bounds = lumen_scene->m_dimensions.max;
	const glm::vec3 diam = pc_ray.max_bounds - pc_ray.min_bounds;
	const float max_comp = glm::max(diam.x, glm::max(diam.y, diam.z));
	const int base_grid_res = int(max_comp / pc_ray.radius);
	pc_ray.grid_res = glm::max(ivec3(diam * float(base_grid_res) / max_comp), ivec3(1));
	pc_ray.total_light_area = lumen_scene->total_light_area;
	pc_ray.light_triangle_count = lumen_scene->total_light_triangle_cnt;

	lumen::RenderGraph* rg = lumen::VulkanBase::render_graph();
	auto get_pipeline_postfix = [&](const std::vector<uint32_t>& spec_consts) {
		std::string res = "-";
		if (spec_consts[0] == 1) {
			res += "SEED";
		}
		if (spec_consts[1] == 1) {
			res += "&LIGHT_FIRST";
		}
		return res;
	};
	auto op_reduce = [&](const std::string& op_name, const std::string& op_shader_name, const std::string& reduce_name,
						 const std::string& reduce_shader_name, const std::vector<uint32_t> spec_data) {
		uint32_t num_wgs = uint32_t((config->num_mlt_threads + 1023) / 1024);
		rg->add_compute(
			  op_name,
			  {.shader = lumen::Shader(op_shader_name), .specialization_data = spec_data, .dims = {num_wgs, 1, 1}})
			.push_constants(&pc_ray)
			.bind(lumen_scene->scene_desc_buffer)
			.zero({mlt_residual_buffer, counter_buffer});
		while (num_wgs != 1) {
			rg->add_compute(reduce_name, {.shader = lumen::Shader(reduce_shader_name),
										  .specialization_data = spec_data,
										  .dims = {num_wgs, 1, 1}})
				.push_constants(&pc_ray)
				.bind(lumen_scene->scene_desc_buffer);
			num_wgs = (uint32_t)(num_wgs + 1023) / 1024;
		}
	};
	auto sum_up_chain_data = [&] {
		op_reduce("OpReduce: Sum0", "src/shaders/integrators/vcmmlt/sum.comp", "OpReduce: Reduce Sum0",
				  "src/shaders/integrators/vcmmlt/reduce_sum.comp", {0});
		op_reduce("OpReduce: Sum1", "src/shaders/integrators/vcmmlt/sum.comp", "OpReduce: Reduce Sum1",
				  "src/shaders/integrators/vcmmlt/reduce_sum.comp", {1});
	};
	std::initializer_list<lumen::ResourceBinding> rt_bindings = {
		output_tex,
		scene_ubo_buffer,
		lumen_scene->scene_desc_buffer,
	};
	std::vector<uint32_t> spec_consts;
	if (!light_first) {
		spec_consts = {1, 0};
	} else {
		spec_consts = {1, 1};
	}
	std::string pipeline_postfix = get_pipeline_postfix(spec_consts);
	// Shoot rays
	std::string pipeline_name = "VCMMLT - Trace " + pipeline_postfix;
	rg->add_rt(pipeline_name,
			   {
				   .shaders = {{"src/shaders/integrators/vcmmlt/vcmmlt_eye.rgen"},
							   {"src/shaders/ray.rmiss"},
							   {"src/shaders/ray_shadow.rmiss"},
							   {"src/shaders/ray.rchit"},
							   {"src/shaders/ray.rahit"}},
				   .specialization_data = spec_consts,
				   .dims = {instance->width * instance->height},
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
	rg->add_rt(pipeline_name,
			   {
				   .shaders = {{"src/shaders/integrators/vcmmlt/vcmmlt_seed.rgen"},
							   {"src/shaders/ray.rmiss"},
							   {"src/shaders/ray_shadow.rmiss"},
							   {"src/shaders/ray.rchit"},
							   {"src/shaders/ray.rahit"}},
				   .specialization_data = spec_consts,
				   .dims = {(uint32_t)config->num_bootstrap_samples},
			   })
		.push_constants(&pc_ray)
		.bind(rt_bindings)
		.bind(lumen_scene->mesh_lights_buffer)
		.bind_texture_array(lumen_scene->scene_textures)
		.bind_tlas(tlas);
	int counter = 0;
	prefix_scan(0, config->num_bootstrap_samples, counter, rg);
	// Calculate CDF
	rg->add_compute("Calculate CDF",
					{.shader = lumen::Shader("src/shaders/integrators/pssmlt/calc_cdf.comp"),
					 .dims = {(uint32_t)std::ceil(config->num_bootstrap_samples / float(1024.0f)), 1, 1}})
		.push_constants(&pc_ray)
		.bind(lumen_scene->scene_desc_buffer);
	// Select seeds
	rg->add_compute("Select Seeds", {.shader = lumen::Shader("src/shaders/integrators/vcmmlt/select_seeds.comp"),
									 .dims = {(uint32_t)std::ceil(config->num_mlt_threads / float(1024.0f)), 1, 1}})
		.push_constants(&pc_ray)
		.bind(lumen_scene->scene_desc_buffer);
	// Fill in the samplers for mutations
	{
		// Fill
		std::string pipeline_name = "VCMMLT - Preprocess " + pipeline_postfix;
		rg->add_rt(pipeline_name,
				   {
					   .shaders = {{"src/shaders/integrators/vcmmlt/vcmmlt_preprocess.rgen"},
								   {"src/shaders/ray.rmiss"},
								   {"src/shaders/ray_shadow.rmiss"},
								   {"src/shaders/ray.rchit"},
								   {"src/shaders/ray.rahit"}},
					   .specialization_data = spec_consts,
					   .dims = {(uint32_t)config->num_mlt_threads},
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
					{.shader = lumen::Shader("src/shaders/integrators/vcmmlt/normalize.comp"), .dims = {1, 1, 1}})
		.push_constants(&pc_ray)
		.bind(lumen_scene->scene_desc_buffer);
	rg->run_and_submit(cmd);
	// Start mutations
	{
		std::string pipeline_name = "VCMMLT - Mutate " + pipeline_postfix;
		auto mutate = [&](uint32_t i) {
			pc_ray.random_num = rand() % UINT_MAX;
			pc_ray.mutation_counter = i;
			// Mutate
			rg->add_rt(pipeline_name,
					   {
						   .shaders = {{"src/shaders/integrators/vcmmlt/vcmmlt_mutate.rgen"},
									   {"src/shaders/ray.rmiss"},
									   {"src/shaders/ray_shadow.rmiss"},
									   {"src/shaders/ray.rchit"},
									   {"src/shaders/ray.rahit"}},
						   .dims = {(uint32_t)config->num_mlt_threads},
					   })
				.push_constants(&pc_ray)
				.zero(mlt_atomicsum_buffer)
				.bind(rt_bindings)
				.bind(lumen_scene->mesh_lights_buffer)
				.bind_texture_array(lumen_scene->scene_textures)
				.bind_tlas(tlas);
			sum_up_chain_data();
			// Normalization
			rg->add_compute(
				  "Calculate Normalization",
				  {.shader = lumen::Shader("src/shaders/integrators/vcmmlt/normalize.comp"), .dims = {1, 1, 1}})
				.push_constants(&pc_ray)
				.bind(lumen_scene->scene_desc_buffer);
		};
		const uint32_t iter_cnt = 100;
		const uint32_t freq = mutation_count / iter_cnt;
		uint32_t cnt = 0;
		for (uint32_t f = 0; f < freq; f++) {
			LUMEN_TRACE("Mutation: {} / {}", cnt, mutation_count);
			cmd.begin();
			for (int i = 0; i < iter_cnt; i++) {
				mutate(cnt++);
			}
			rg->run(cmd.handle);
			rg->submit(cmd);
		}
		const uint32_t rem = mutation_count % iter_cnt;
		if (rem) {
			cmd.begin();
			for (uint32_t i = 0; i < rem; i++) {
				mutate(cnt++);
			}
			rg->run(cmd.handle);
			rg->submit(cmd);
		}
	}
	// Compositions
	rg->add_compute("Composition",
					{.shader = lumen::Shader("src/shaders/integrators/vcmmlt/composite.comp"),
					 .dims = {(uint32_t)std::ceil(instance->width * instance->height / float(1024.0f)), 1, 1}})
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

void VCMMLT::prefix_scan(int level, int num_elems, int& counter, lumen::RenderGraph* rg) {
	const bool scan_sums = level > 0;
	int num_wgs = std::max(1, (int)ceil(num_elems / (2 * 1024.0f)));
	int num_grids = num_wgs - int((num_elems % 2048) != 0);
	pc_compute.num_elems = num_elems;
	auto scan = [&](int num_wgs, int idx) {
		++counter;
		rg->add_compute("PrefixScan - Scan",
						{.shader = lumen::Shader("src/shaders/integrators/pssmlt/prefix_scan.comp"),
						 .dims = {(uint32_t)num_wgs, 1, 1}})
			.push_constants(&pc_compute)
			.bind(lumen_scene->scene_desc_buffer);
	};
	auto uniform_add = [&](int num_wgs, int output_idx) {
		++counter;
		rg->add_compute("PrefixScan - Uniform Add",
						{.shader = lumen::Shader("src/shaders/integrators/pssmlt/uniform_add.comp"),
						 .dims = {(uint32_t)num_wgs, 1, 1}})
			.push_constants(&pc_compute)
			.bind(lumen_scene->scene_desc_buffer);
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

void VCMMLT::destroy() {
	Integrator::destroy();
	std::vector<lumen::Buffer*> buffer_list = {
		&bootstrap_buffer,	  &cdf_buffer,			&cdf_sum_buffer,
		&seeds_buffer,		  &mlt_samplers_buffer, &light_primary_samples_buffer,
		&mlt_col_buffer,	  &chain_stats_buffer,	&splat_buffer,
		&past_splat_buffer,	  &light_path_buffer,	&light_path_cnt_buffer,
		&tmp_col_buffer,	  &photon_buffer,		&mlt_atomicsum_buffer,
		&mlt_residual_buffer, &counter_buffer};
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
