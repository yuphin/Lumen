#include "LumenPCH.h"
#include "VCMMLT.h"
static bool use_vm = true;
static float vcm_radius_factor = 0.1f;
static bool light_first = true;
void VCMMLT::init() {
	Integrator::init();
	max_depth = 6;
	mutations_per_pixel = 200.0f;
	sky_col = vec3(0, 0, 0);
	num_mlt_threads = 1600 * 900 / 2;
	num_bootstrap_samples = 1600 * 900 / 2;
	mutation_count = int(instance->width * instance->height * mutations_per_pixel / float(num_mlt_threads));
	light_path_rand_count = std::max(7 + 2 * max_depth, 3 + 6 * max_depth);

	// MLTVCM buffers
	bootstrap_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		num_bootstrap_samples * sizeof(BootstrapSample)
	);

	cdf_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		num_bootstrap_samples * 4
	);

	bootstrap_cpu.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		num_bootstrap_samples * sizeof(BootstrapSample)
	);


	cdf_cpu.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		num_bootstrap_samples * 4
	);

	cdf_sum_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		sizeof(float)
	);

	seeds_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		num_mlt_threads * sizeof(VCMMLTSeedData)
	);

	light_primary_samples_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		num_mlt_threads * light_path_rand_count * sizeof(PrimarySample) * 2
	);

	mlt_samplers_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		num_mlt_threads * sizeof(VCMMLTSampler) * 2
	);

	mlt_col_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		instance->width * instance->height * 3 * sizeof(float)
	);

	chain_stats_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		2 * sizeof(ChainData)
	);

	splat_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		num_mlt_threads * (max_depth * (max_depth + 1)) * sizeof(Splat) * 2
	);

	past_splat_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		num_mlt_threads * (max_depth * (max_depth + 1)) * sizeof(Splat) * 2
	);

	light_path_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		instance->width * instance->height * (max_depth + 1) * sizeof(VCMVertex));

	light_path_cnt_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		instance->width * instance->height * sizeof(float)
	);

	tmp_col_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		instance->width * instance->height * sizeof(float) * 3);

	photon_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		10 * instance->width * instance->height * sizeof(PhotonHash)
	);

	mlt_atomicsum_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		num_mlt_threads * sizeof(SumData) * 2
	);

	mlt_residual_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		num_mlt_threads * sizeof(SumData)
	);

	counter_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		sizeof(int)
	);


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
								   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
								   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
								   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
								   num_blocks * 4);
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

	scene_desc_buffer.create(&instance->vkb.ctx,
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
							 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							 VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneDesc),
							 &desc, true);
	create_blas();
	create_tlas();
	create_offscreen_resources();
	create_descriptors();
	create_rt_pipelines();
	create_compute_pipelines();
	pc_ray.total_light_area = 0;
	pc_ray.frame_num = 0;
	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;
	pc_ray.mutations_per_pixel = mutations_per_pixel;
	pc_ray.num_mlt_threads = num_mlt_threads;
}

void VCMMLT::render() {
	LUMEN_TRACE("Rendering sample {}...", sample_cnt);
	const float ppm_base_radius = 0.25f;
	CommandBuffer cmd(&instance->vkb.ctx, /*start*/ true);
	VkClearValue clear_color = { 0.25f, 0.25f, 0.25f, 1.0f };
	VkClearValue clear_depth = { 1.0f, 0 };
	VkViewport viewport = vk::viewport((float)instance->width, (float)instance->height, 0.0f, 1.0f);
	VkClearValue clear_values[] = { clear_color, clear_depth };
	pc_ray.light_pos = scene_ubo.light_pos;
	pc_ray.light_type = 0;
	pc_ray.light_intensity = 10;
	pc_ray.num_lights = int(lights.size());
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = max_depth;
	pc_ray.sky_col = sky_col;
	// VCMMLT related constants
	pc_ray.use_vm = use_vm;
	pc_ray.light_rand_count = light_path_rand_count;
	pc_ray.random_num = rand() % UINT_MAX;
	pc_ray.num_bootstrap_samples = num_bootstrap_samples;
	pc_ray.radius = lumen_scene.m_dimensions.radius * vcm_radius_factor / 100.f;
	pc_ray.radius /= (float)pow((double)pc_ray.frame_num + 1, 0.5 * (1 - 2.0 / 3));
	pc_ray.min_bounds = lumen_scene.m_dimensions.min;
	pc_ray.max_bounds = lumen_scene.m_dimensions.max;
	pc_ray.ppm_base_radius = ppm_base_radius;
	const glm::vec3 diam = pc_ray.max_bounds - pc_ray.min_bounds;
	const float max_comp = glm::max(diam.x, glm::max(diam.y, diam.z));
	const int base_grid_res = int(max_comp / pc_ray.radius);
	pc_ray.grid_res = glm::max(ivec3(diam * float(base_grid_res) / max_comp), ivec3(1));
	vkCmdFillBuffer(cmd.handle, chain_stats_buffer.handle, 0, chain_stats_buffer.size, 0);
	vkCmdFillBuffer(cmd.handle, mlt_atomicsum_buffer.handle, 0, mlt_atomicsum_buffer.size, 0);
	std::array<VkBufferMemoryBarrier, 2> barriers = {
						buffer_barrier(chain_stats_buffer.handle,
								  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
								  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
						buffer_barrier(mlt_atomicsum_buffer.handle,
								  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
								  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
	};
	vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
						 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
						 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, barriers.size(), barriers.data(), 0, 0);
	if (use_vm) {
		vkCmdFillBuffer(cmd.handle, photon_buffer.handle, 0, photon_buffer.size, 0);
		auto barrier = buffer_barrier(photon_buffer.handle,
									  VK_ACCESS_TRANSFER_WRITE_BIT,
									  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &barrier, 0, 0);
	}
	auto sum_up_chain_data = [&] {
		auto wg_x = 1024;
		auto wg_y = 1;
		const auto num_wg_x = (uint32_t)ceil(num_mlt_threads / float(wg_x));
		const auto num_wg_y = 1;
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, sum0_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, sum1_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, sum_reduce0_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, sum_reduce1_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdPushConstants(cmd.handle, sum0_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_COMPUTE_BIT,
						   0, sizeof(PushConstantRay), &pc_ray);
		vkCmdPushConstants(cmd.handle, sum1_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_COMPUTE_BIT,
						   0, sizeof(PushConstantRay), &pc_ray);
		vkCmdPushConstants(cmd.handle, sum_reduce0_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_COMPUTE_BIT,
						   0, sizeof(PushConstantRay), &pc_ray);
		vkCmdPushConstants(cmd.handle, sum_reduce1_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_COMPUTE_BIT,
						   0, sizeof(PushConstantRay), &pc_ray);
		reduce(cmd.handle, mlt_residual_buffer, counter_buffer, *sum0_pipeline, *sum_reduce0_pipeline,
			   num_mlt_threads);
		auto barrier = buffer_barrier(chain_stats_buffer.handle,
									  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
									  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 1,
							 &barrier, 0, 0);
		reduce(cmd.handle, mlt_residual_buffer, counter_buffer, *sum1_pipeline, *sum_reduce1_pipeline,
			   num_mlt_threads);
		barrier = buffer_barrier(chain_stats_buffer.handle,
								 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
								 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 1,
							 &barrier, 0, 0);
	};
	// Shoot rays
	{
		auto trace_pipeline = !light_first ? eye_pipeline.get() : light_pipeline.get();
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
						  trace_pipeline->handle);
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, trace_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdPushConstants(cmd.handle, trace_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_RAYGEN_BIT_KHR |
						   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
						   VK_SHADER_STAGE_MISS_BIT_KHR,
						   0, sizeof(PushConstantRay), &pc_ray);
		auto& regions = trace_pipeline->get_rt_regions();
		vkCmdTraceRaysKHR(cmd.handle, &regions[0], &regions[1], &regions[2], &regions[3],
						  instance->width, instance->height, 1);
		std::array<VkBufferMemoryBarrier, 3> barriers = {
									  buffer_barrier(light_path_buffer.handle,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
									  buffer_barrier(light_path_cnt_buffer.handle,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
									  buffer_barrier(photon_buffer.handle,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)
		};
		vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, barriers.size(), barriers.data(), 0, 0);
	}
	// Start bootstrap sampling
	{
		auto seed_pipeline = !light_first ? seed1_pipeline.get() : seed2_pipeline.get();
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
						  seed_pipeline->handle);
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, seed_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdPushConstants(cmd.handle, seed_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_RAYGEN_BIT_KHR |
						   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
						   VK_SHADER_STAGE_MISS_BIT_KHR,
						   0, sizeof(PushConstantRay), &pc_ray);
		auto& regions = seed_pipeline->get_rt_regions();
		vkCmdTraceRaysKHR(cmd.handle, &regions[0], &regions[1], &regions[2], &regions[3],
						  num_bootstrap_samples, 1, 1);
		auto barrier = buffer_barrier(bootstrap_buffer.handle,
									  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
									  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 1,
							 &barrier, 0, 0);
	}
	prefix_scan(0, num_bootstrap_samples, cmd);

	// Calculate CDF
	{
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, calc_cdf_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
						  calc_cdf_pipeline->handle);
		vkCmdPushConstants(cmd.handle, calc_cdf_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantRay), &pc_ray);
		auto num_wgs = (num_bootstrap_samples + 1023) / 1024;
		vkCmdDispatch(cmd.handle, num_wgs, 1, 1);
		std::array<VkBufferMemoryBarrier, 2> barriers = {
			buffer_barrier(cdf_sum_buffer.handle,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
			buffer_barrier(cdf_buffer.handle,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)
		};
		vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, (uint32_t)barriers.size(),
							 barriers.data(), 0, 0);
	}

	// Select seeds
	{
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, select_seeds_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
						  select_seeds_pipeline->handle);
		vkCmdPushConstants(cmd.handle, select_seeds_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantRay), &pc_ray);
		uint32_t num_wgs = (num_mlt_threads + 1023) / 1024;
		vkCmdDispatch(cmd.handle, num_wgs, 1, 1);
		auto barrier = buffer_barrier(seeds_buffer.handle,
									  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
									  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 1,
							 &barrier, 0, 0);
	}
	// Fill in the samplers for mutations
	{
		// Fill
		auto preprocess_pipeline = !light_first ? preprocess1_pipeline.get() : preprocess2_pipeline.get();
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
						  preprocess_pipeline->handle);
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, preprocess_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdPushConstants(cmd.handle, preprocess_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_RAYGEN_BIT_KHR |
						   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
						   VK_SHADER_STAGE_MISS_BIT_KHR,
						   0, sizeof(PushConstantRay), &pc_ray);
		auto& preprocess_regions = preprocess_pipeline->get_rt_regions();
		vkCmdTraceRaysKHR(cmd.handle, &preprocess_regions[0], &preprocess_regions[1],
						  &preprocess_regions[2], &preprocess_regions[3], num_mlt_threads, 1, 1);
		const std::array<VkBufferMemoryBarrier, 3> preprocess_barriers = {
		 buffer_barrier(seeds_buffer.handle,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
		buffer_barrier(mlt_samplers_buffer.handle,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
		buffer_barrier(mlt_atomicsum_buffer.handle,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)
		};
		vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0,
							 (uint32_t)preprocess_barriers.size(),
							 preprocess_barriers.data(), 0, 0);
		// Sum up chain stats
		sum_up_chain_data();
	}


	// Calculate normalization factor
	{
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, normalize_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
						  normalize_pipeline->handle);
		vkCmdPushConstants(cmd.handle, normalize_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantRay), &pc_ray);
		vkCmdDispatch(cmd.handle, 1, 1, 1);
	}
	cmd.submit();

	// Start mutations
	{
		auto mutate = [&](uint32_t i) {
			pc_ray.random_num = rand() % UINT_MAX;
			pc_ray.mutation_counter = i;
			// Mutate
			{
				auto barrier = buffer_barrier(mlt_atomicsum_buffer.handle,
											  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
											  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
				vkCmdFillBuffer(cmd.handle, mlt_atomicsum_buffer.handle, 0, mlt_atomicsum_buffer.size, 0);
				vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
									 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
									 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &barrier, 0, 0);
				auto mutate_pipeline = !light_first ? mutate1_pipeline.get() : mutate2_pipeline.get();
				vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
								  mutate_pipeline->handle);
				vkCmdBindDescriptorSets(
					cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, mutate_pipeline->pipeline_layout,
					0, 1, &desc_set, 0, nullptr);
				vkCmdPushConstants(cmd.handle, mutate_pipeline->pipeline_layout,
								   VK_SHADER_STAGE_RAYGEN_BIT_KHR |
								   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
								   VK_SHADER_STAGE_MISS_BIT_KHR,
								   0, sizeof(PushConstantRay), &pc_ray);
				auto& regions = mutate_pipeline->get_rt_regions();
				vkCmdTraceRaysKHR(cmd.handle, &regions[0], &regions[1], &regions[2],
								  &regions[3], num_mlt_threads, 1, 1);
				std::array<VkBufferMemoryBarrier, 6> mutation_barriers = {
					buffer_barrier(splat_buffer.handle,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
					buffer_barrier(past_splat_buffer.handle,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
					buffer_barrier(mlt_samplers_buffer.handle,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
					buffer_barrier(seeds_buffer.handle,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
					buffer_barrier(mlt_col_buffer.handle,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
					buffer_barrier(mlt_atomicsum_buffer.handle,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
				};
				vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
									 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0,
									 (uint32_t)mutation_barriers.size(),
									 mutation_barriers.data(), 0, 0);
				// Sum up chain stats
				sum_up_chain_data();
			}
			// Normalization
			{
				vkCmdBindDescriptorSets(
					cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, normalize_pipeline->pipeline_layout,
					0, 1, &desc_set, 0, nullptr);
				vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
								  normalize_pipeline->handle);
				vkCmdPushConstants(cmd.handle, normalize_pipeline->pipeline_layout,
								   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantRay), &pc_ray);
				vkCmdDispatch(cmd.handle, 1, 1, 1);
			}
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
			cmd.submit();
		}
		const uint32_t rem = mutation_count % iter_cnt;
		if (rem) {
			cmd.begin();
			for (uint32_t i = 0; i < rem; i++) {
				mutate(cnt++);
			}
			cmd.submit();
		}
	}
	// Compositions
	cmd.begin();
	{
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, composite_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
						  composite_pipeline->handle);
		vkCmdPushConstants(cmd.handle, composite_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantRay), &pc_ray);
		int num_wgs = (instance->width * instance->height + 1023) / 1024;
		vkCmdDispatch(cmd.handle, num_wgs, 1, 1);
	}
	cmd.submit();
	//light_first ^= true;
	sample_cnt++;
}

bool VCMMLT::gui() {
	bool result = false;
	result |= ImGui::Checkbox("Enable Light-first ordering(default = eye)", &light_first);
	if (light_first) {
		result |= ImGui::Checkbox("Enable VM", &use_vm);
	}
	return result;
}

bool VCMMLT::update() {
	pc_ray.frame_num++;
	glm::vec3 translation{};
	float trans_speed = 0.01f;
	glm::vec3 front;
	if (instance->window->is_key_held(KeyInput::KEY_LEFT_SHIFT)) {
		trans_speed *= 4;
	}

	front.x = cos(glm::radians(camera->rotation.x)) *
		sin(glm::radians(camera->rotation.y));
	front.y = sin(glm::radians(camera->rotation.x));
	front.z = cos(glm::radians(camera->rotation.x)) *
		cos(glm::radians(camera->rotation.y));
	front = glm::normalize(-front);
	if (instance->window->is_key_held(KeyInput::KEY_W)) {
		camera->position += front * trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::KEY_A)) {
		camera->position -=
			glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) *
			trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::KEY_S)) {
		camera->position -= front * trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::KEY_D)) {
		camera->position +=
			glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) *
			trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::SPACE)) {
		// Right
		auto right =
			glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
		auto up = glm::cross(right, front);
		camera->position += up * trans_speed;
		updated = true;
	}
	if (instance->window->is_key_held(KeyInput::KEY_LEFT_CONTROL)) {
		auto right =
			glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
		auto up = glm::cross(right, front);
		camera->position -= up * trans_speed;
		updated = true;
	}
	bool result = false;
	if (updated) {
		result = true;
		pc_ray.frame_num = 0;
		updated = false;
	}
	update_uniform_buffers();
	return result;
}

void VCMMLT::create_offscreen_resources() {
	// Create offscreen image for output
	TextureSettings settings;
	settings.usage_flags =
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	settings.base_extent = { (uint32_t)instance->width, (uint32_t)instance->height, 1 };
	settings.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	output_tex.create_empty_texture(&instance->vkb.ctx, settings,
									VK_IMAGE_LAYOUT_GENERAL);
	CommandBuffer cmd(&instance->vkb.ctx, true);
	transition_image_layout(cmd.handle, output_tex.img,
							VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	cmd.submit();
}

void VCMMLT::create_descriptors() {
	constexpr int TLAS_BINDING = 0;
	constexpr int IMAGE_BINDING = 1;
	constexpr int INSTANCE_BINDING = 2;
	constexpr int UNIFORM_BUFFER_BINDING = 3;
	constexpr int SCENE_DESC_BINDING = 4;
	constexpr int TEXTURES_BINDING = 5;
	constexpr int LIGHTS_BINDING = 6;

	auto num_textures = textures.size();
	std::vector<VkDescriptorPoolSize> pool_sizes = {
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
								 num_textures),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
								 1),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1) };
	auto descriptor_pool_ci =
		vk::descriptor_pool_CI(pool_sizes.size(), pool_sizes.data(),
							   instance->vkb.ctx.swapchain_images.size());

	vk::check(vkCreateDescriptorPool(instance->vkb.ctx.device, &descriptor_pool_ci,
			  nullptr, &desc_pool),
			  "Failed to create descriptor pool");

	// Uniform buffer descriptors
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
			TLAS_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_COMPUTE_BIT,
			IMAGE_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
				VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			INSTANCE_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT |
				VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			UNIFORM_BUFFER_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
				VK_SHADER_STAGE_RAYGEN_BIT_KHR |
				VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
			SCENE_DESC_BINDING),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_FRAGMENT_BIT |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
				VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
			TEXTURES_BINDING, num_textures),
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR,
			LIGHTS_BINDING)
	};
	auto set_layout_ci = vk::descriptor_set_layout_CI(
		set_layout_bindings.data(), set_layout_bindings.size());
	vk::check(vkCreateDescriptorSetLayout(instance->vkb.ctx.device, &set_layout_ci,
			  nullptr, &desc_set_layout),
			  "Failed to create escriptor set layout");
	VkDescriptorSetAllocateInfo set_allocate_info{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	set_allocate_info.descriptorPool = desc_pool;
	set_allocate_info.descriptorSetCount = 1;
	set_allocate_info.pSetLayouts = &desc_set_layout;
	vkAllocateDescriptorSets(instance->vkb.ctx.device, &set_allocate_info, &desc_set);

	// Update descriptors
	VkAccelerationStructureKHR tlas = instance->vkb.tlas.accel;
	VkWriteDescriptorSetAccelerationStructureKHR desc_as_info{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	desc_as_info.accelerationStructureCount = 1;
	desc_as_info.pAccelerationStructures = &tlas;

	// TODO: Abstraction
	std::vector<VkWriteDescriptorSet> writes{
		vk::write_descriptor_set(desc_set,
								 VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
								 TLAS_BINDING, &desc_as_info),
		vk::write_descriptor_set(desc_set, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
								 IMAGE_BINDING,
								 &output_tex.descriptor_image_info),
		vk::write_descriptor_set(desc_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
								 INSTANCE_BINDING,
								 &prim_lookup_buffer.descriptor),
	   vk::write_descriptor_set(desc_set,
								VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
								UNIFORM_BUFFER_BINDING, &scene_ubo_buffer.descriptor),
	   vk::write_descriptor_set(desc_set,
								VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
								SCENE_DESC_BINDING, &scene_desc_buffer.descriptor) };
	std::vector<VkDescriptorImageInfo> image_infos;
	for (auto& tex : textures) {
		image_infos.push_back(tex.descriptor_image_info);
	}
	writes.push_back(vk::write_descriptor_set(desc_set,
					 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, TEXTURES_BINDING,
					 image_infos.data(), (uint32_t)image_infos.size()));
	if (lights.size()) {
		writes.push_back(vk::write_descriptor_set(
			desc_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, LIGHTS_BINDING,
			&mesh_lights_buffer.descriptor));
	}
	vkUpdateDescriptorSets(
		instance->vkb.ctx.device, static_cast<uint32_t>(writes.size()),
		writes.data(), 0, nullptr);
};


void VCMMLT::create_blas() {
	std::vector<BlasInput> blas_inputs;
	auto vertex_address =
		get_device_address(instance->vkb.ctx.device, vertex_buffer.handle);
	auto idx_address = get_device_address(instance->vkb.ctx.device, index_buffer.handle);
	for (auto& prim_mesh : lumen_scene.prim_meshes) {
		BlasInput geo = to_vk_geometry(prim_mesh, vertex_address, idx_address);
		blas_inputs.push_back({ geo });
	}
	instance->vkb.build_blas(blas_inputs,
							 VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void VCMMLT::create_tlas() {
	std::vector<VkAccelerationStructureInstanceKHR> tlas;
	float total_light_triangle_area = 0.0f;
	const auto& indices = lumen_scene.indices;
	const auto& vertices = lumen_scene.positions;
	for (const auto& pm : lumen_scene.prim_meshes) {
		VkAccelerationStructureInstanceKHR ray_inst{};
		ray_inst.transform = to_vk_matrix(pm.world_matrix);
		ray_inst.instanceCustomIndex = pm.prim_idx;
		ray_inst.accelerationStructureReference =
			instance->vkb.get_blas_device_address(pm.prim_idx);
		ray_inst.flags =
			VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		ray_inst.mask = 0xFF;
		ray_inst.instanceShaderBindingTableRecordOffset =
			0; // We will use the same hit group for all objects
		tlas.emplace_back(ray_inst);
	}

	for (auto& l : lights) {
		if (l.light_flags == LIGHT_AREA) {
			const auto& pm = lumen_scene.prim_meshes[l.prim_mesh_idx];
			l.world_matrix = pm.world_matrix;
			auto& idx_base_offset = pm.first_idx;
			auto& vtx_offset = pm.vtx_offset;
			for (uint32_t i = 0; i < l.num_triangles; i++) {
				auto idx_offset = idx_base_offset + 3 * i;
				glm::ivec3 ind = { indices[idx_offset], indices[idx_offset + 1],
								  indices[idx_offset + 2] };
				ind += glm::vec3{ vtx_offset, vtx_offset, vtx_offset };
				const vec3 v0 =
					pm.world_matrix * glm::vec4(vertices[ind.x], 1.0);
				const vec3 v1 =
					pm.world_matrix * glm::vec4(vertices[ind.y], 1.0);
				const vec3 v2 =
					pm.world_matrix * glm::vec4(vertices[ind.z], 1.0);
				float area = 0.5f * glm::length(glm::cross(v1 - v0, v2 - v0));
				total_light_triangle_area += area;
			}
		}

	}

	if (lights.size()) {
		mesh_lights_buffer.create(
			&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
			lights.size() * sizeof(Light), lights.data(), true);
	}

	pc_ray.total_light_area += total_light_triangle_area;
	if (total_light_triangle_cnt > 0) {
		pc_ray.light_triangle_count = total_light_triangle_cnt;
	}
	instance->vkb.build_tlas(tlas,
							 VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void VCMMLT::create_rt_pipelines() {
	enum StageIndices {
		Raygen = 0,
		CMiss = 1,
		AMiss = 2,
		ClosestHit = 3,
		AnyHit = 4,
		ShaderGroupCount = 5
	};
	RTPipelineSettings settings;
	std::vector<Shader> shaders{
								{"src/shaders/integrators/vcmmlt/vcmmlt_eye.rgen"},
								{"src/shaders/integrators/vcmmlt/vcmmlt_seed.rgen"},
								{"src/shaders/integrators/vcmmlt/vcmmlt_preprocess.rgen"},
								{"src/shaders/integrators/vcmmlt/vcmmlt_mutate.rgen"},
								{"src/shaders/integrators/ray.rmiss"},
								{"src/shaders/integrators/ray_shadow.rmiss"},
								{"src/shaders/integrators/ray.rchit"},
								{"src/shaders/integrators/ray.rahit"} };
	for (auto& shader : shaders) {
		shader.compile();
	}
	settings.ctx = &instance->vkb.ctx;
	settings.rt_props = rt_props;
	// All stages

	std::vector<VkPipelineShaderStageCreateInfo> stages;
	stages.resize(ShaderGroupCount);

	VkPipelineShaderStageCreateInfo stage{
		VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stage.pName = "main";
	// Raygen
	stage.module = shaders[0].create_vk_shader_module(instance->vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	stages[Raygen] = stage;
	// Miss
	stage.module = shaders[4].create_vk_shader_module(instance->vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[CMiss] = stage;

	stage.module = shaders[5].create_vk_shader_module(instance->vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[AMiss] = stage;
	// Hit Group - Closest Hit
	stage.module = shaders[6].create_vk_shader_module(instance->vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[ClosestHit] = stage;
	// Hit Group - Any hit
	stage.module = shaders[7].create_vk_shader_module(instance->vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	stages[AnyHit] = stage;


	// Shader groups
	VkRayTracingShaderGroupCreateInfoKHR group{
		VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
	group.anyHitShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = VK_SHADER_UNUSED_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.intersectionShader = VK_SHADER_UNUSED_KHR;

	// Raygen
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = Raygen;
	settings.groups.push_back(group);

	// Miss
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = CMiss;
	settings.groups.push_back(group);

	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	group.generalShader = AMiss;
	settings.groups.push_back(group);

	// closest hit shader
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.closestHitShader = ClosestHit;
	settings.groups.push_back(group);

	// Any hit shader
	group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	group.generalShader = VK_SHADER_UNUSED_KHR;
	group.anyHitShader = AnyHit;
	settings.groups.push_back(group);

	settings.push_consts.push_back({ VK_SHADER_STAGE_RAYGEN_BIT_KHR |
										 VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
										 VK_SHADER_STAGE_MISS_BIT_KHR,
									 0, sizeof(PushConstantRay) });
	settings.desc_layouts = { desc_set_layout };
	settings.stages = stages;
	eye_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	light_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	seed1_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	seed2_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	preprocess1_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	preprocess2_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	mutate1_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	mutate2_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	// TODO: Move shaders into sets during recompilation
	settings.shaders = { shaders[0], shaders[4], shaders[5], shaders[6], shaders[7] };
	eye_pipeline->create_rt_pipeline(settings, { 0, 0 });
	light_pipeline->create_rt_pipeline(settings, { 0, 1 });
	vkDestroyShaderModule(instance->vkb.ctx.device, stages[Raygen].module, nullptr);
	stages[Raygen].module = shaders[1].create_vk_shader_module(instance->vkb.ctx.device);
	settings.shaders[0] = shaders[1];
	settings.stages = stages;
	seed1_pipeline->create_rt_pipeline(settings, { 1, 0 });
	seed2_pipeline->create_rt_pipeline(settings, { 1, 1 });
	vkDestroyShaderModule(instance->vkb.ctx.device, stages[Raygen].module, nullptr);
	stages[Raygen].module = shaders[2].create_vk_shader_module(instance->vkb.ctx.device);
	settings.shaders[0] = shaders[2];
	settings.stages = stages;
	preprocess1_pipeline->create_rt_pipeline(settings, { 0, 0 });
	preprocess2_pipeline->create_rt_pipeline(settings, { 0, 1 });
	vkDestroyShaderModule(instance->vkb.ctx.device, stages[Raygen].module, nullptr);
	stages[Raygen].module = shaders[3].create_vk_shader_module(instance->vkb.ctx.device);
	settings.shaders[0] = shaders[3];
	settings.stages = stages;
	mutate1_pipeline->create_rt_pipeline(settings, { 0, 0 });
	mutate2_pipeline->create_rt_pipeline(settings, { 0, 1 });
	for (auto& s : settings.stages) {
		vkDestroyShaderModule(instance->vkb.ctx.device, s.module, nullptr);
	}
}

void VCMMLT::create_compute_pipelines() {
	calc_cdf_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	select_seeds_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	composite_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	prefix_scan_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	uniform_add_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	normalize_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	sum0_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	sum1_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	sum_reduce0_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	sum_reduce1_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	std::vector<Shader> shaders = {
		{"src/shaders/integrators/pssmlt/calc_cdf.comp"},
		{"src/shaders/integrators/vcmmlt/select_seeds.comp"},
		{"src/shaders/integrators/vcmmlt/composite.comp"},
		{"src/shaders/integrators/pssmlt/prefix_scan.comp"},
		{"src/shaders/integrators/pssmlt/uniform_add.comp"},
		{"src/shaders/integrators/vcmmlt/normalize.comp"},
		{"src/shaders/integrators/vcmmlt/sum.comp"},
		{"src/shaders/integrators/vcmmlt/reduce_sum.comp"},
	};
	for (auto& shader : shaders) {
		shader.compile();
	}
	ComputePipelineSettings settings;
	settings.desc_sets = &desc_set_layout;
	settings.desc_set_layout_cnt = 1;
	settings.push_const_size = sizeof(PushConstantRay);
	settings.shader = shaders[0];
	calc_cdf_pipeline->create_compute_pipeline(settings);
	settings.shader = shaders[1];
	select_seeds_pipeline->create_compute_pipeline(settings);
	settings.shader = shaders[2];
	composite_pipeline->create_compute_pipeline(settings);
	settings.push_const_size = sizeof(PushConstantCompute);
	settings.shader = shaders[3];
	prefix_scan_pipeline->create_compute_pipeline(settings);
	settings.shader = shaders[4];
	uniform_add_pipeline->create_compute_pipeline(settings);
	settings.shader = shaders[5];
	settings.push_const_size = sizeof(PushConstantRay);
	normalize_pipeline->create_compute_pipeline(settings);
	settings.shader = shaders[6];
	settings.specialization_data = { 0 };
	sum0_pipeline->create_compute_pipeline(settings);
	settings.specialization_data = { 1 };
	sum1_pipeline->create_compute_pipeline(settings);
	settings.shader = shaders[7];
	settings.specialization_data = { 0 };
	sum_reduce0_pipeline->create_compute_pipeline(settings);
	settings.specialization_data = { 1 };
	sum_reduce1_pipeline->create_compute_pipeline(settings);
}

void VCMMLT::prefix_scan(int level, int num_elems, CommandBuffer& cmd) {
	const bool scan_sums = level > 0;
	int num_wgs = std::max(1, (int)ceil(num_elems / (2 * 1024.0f)));
	int num_grids = num_wgs - int((num_elems % 2048) != 0);
	pc_compute.num_elems = num_elems;
	auto scan = [&](int num_wgs, int idx) {
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, prefix_scan_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
						  prefix_scan_pipeline->handle);
		vkCmdPushConstants(cmd.handle, prefix_scan_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantCompute), &pc_compute);
		vkCmdDispatch(cmd.handle, num_wgs, 1, 1);
		if (scan_sums) {
			auto barrier = buffer_barrier(block_sums[idx].handle,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 1,
								 &barrier, 0, 0);
		} else {
			auto barrier = buffer_barrier(cdf_buffer.handle,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 1,
								 &barrier, 0, 0);
		}

	};
	auto uniform_add = [&](int num_wgs, int output_idx) {
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, uniform_add_pipeline->pipeline_layout,
			0, 1, &desc_set, 0, nullptr);
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
						  uniform_add_pipeline->handle);
		vkCmdPushConstants(cmd.handle, uniform_add_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantCompute), &pc_compute);
		vkCmdDispatch(cmd.handle, num_wgs, 1, 1);
		if (scan_sums) {
			auto barrier = buffer_barrier(block_sums[output_idx].handle,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 1,
								 &barrier, 0, 0);
		} else {
			auto barrier = buffer_barrier(cdf_buffer.handle,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
										  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
			vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
								 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 1,
								 &barrier, 0, 0);
		}
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
		prefix_scan(level + 1, num_wgs, cmd);
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
	const auto device = instance->vkb.ctx.device;
	Integrator::destroy();
	std::vector<Buffer*> buffer_list = {
	  &bootstrap_buffer,
	  &cdf_buffer,
	  &cdf_sum_buffer,
	  &seeds_buffer,
	  &mlt_samplers_buffer,
	  &light_primary_samples_buffer,
	  &mlt_col_buffer,
	  &chain_stats_buffer,
	  &splat_buffer,
	  &past_splat_buffer,
	  &light_path_buffer,
	  &light_path_cnt_buffer,
	  &tmp_col_buffer,
	  &photon_buffer,
	  &mlt_atomicsum_buffer,
	  &mlt_residual_buffer,
	  &counter_buffer
	};
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
	std::vector<Pipeline*> pipeline_list = {
		eye_pipeline.get(),
		light_pipeline.get(),
		seed1_pipeline.get(),
		seed2_pipeline.get(),
		preprocess1_pipeline.get(),
		preprocess2_pipeline.get(),
		mutate1_pipeline.get(),
		mutate2_pipeline.get(),
		calc_cdf_pipeline.get(),
		select_seeds_pipeline.get(),
		composite_pipeline.get(),
		prefix_scan_pipeline.get(),
		uniform_add_pipeline.get(),
		normalize_pipeline.get(),
		sum0_pipeline.get(),
		sum1_pipeline.get(),
		sum_reduce0_pipeline.get(),
		sum_reduce1_pipeline.get()
	};
	for (auto p : pipeline_list) {
		p->cleanup();
	}
	vkDestroyDescriptorSetLayout(device, desc_set_layout, nullptr);
	vkDestroyDescriptorPool(device, desc_pool, nullptr);
}

void VCMMLT::reload() {
	eye_pipeline->reload();
	light_pipeline->reload();
	preprocess1_pipeline->reload();
	preprocess2_pipeline->reload();
	mutate1_pipeline->reload();
	mutate2_pipeline->reload();
	calc_cdf_pipeline->reload();
	select_seeds_pipeline->reload();
	composite_pipeline->reload();
	prefix_scan_pipeline->reload();
	uniform_add_pipeline->reload();
	normalize_pipeline->reload();
	sum0_pipeline->reload();
	sum1_pipeline->reload();
	sum_reduce0_pipeline->reload();
	sum_reduce1_pipeline->reload();
}
