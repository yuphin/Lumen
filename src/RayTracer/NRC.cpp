#include "LumenPCH.h"
#include "NRC.h"
const int COLLECTION_FRAME_COUNT = 1;
const int TILE_SIZE = 4;

#define CUDADRV_CHECK(call)                                                                                          \
	do {                                                                                                             \
		CUresult error = call;                                                                                       \
		if (error != CUDA_SUCCESS) {                                                                                 \
			std::stringstream ss;                                                                                    \
			const char *errMsg = "failed to get an error message.";                                                  \
			cuGetErrorString(error, &errMsg);                                                                        \
			ss << "CUDA call (" << #call << " ) failed with error: '" << errMsg << "' (" __FILE__ << ":" << __LINE__ \
			   << ")\n";                                                                                             \
			throw std::runtime_error(ss.str().c_str());                                                              \
		}                                                                                                            \
	} while (0)

void NRC::init() {
	Integrator::init();
	SceneDesc desc;
	desc.vertex_addr = vertex_buffer.get_device_address();
	desc.index_addr = index_buffer.get_device_address();
	desc.normal_addr = normal_buffer.get_device_address();
	desc.uv_addr = uv_buffer.get_device_address();
	desc.material_addr = materials_buffer.get_device_address();
	desc.prim_info_addr = prim_lookup_buffer.get_device_address();

	max_samples_count = instance->width * instance->height * COLLECTION_FRAME_COUNT * (config->path_length - 2) /
						(TILE_SIZE * TILE_SIZE);

	max_samples_count = (max_samples_count + 1023) / 1024 * 1024;

	neural_radiance_cache.initialize(PositionEncoding::HashGrid, 5, 1e-3f);
	CUDADRV_CHECK(cuStreamCreate(&cu_stream, 0));
	radiance_query_buffer_pong.create_external(&instance->vkb.ctx,
											   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
												   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
											   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
											   max_samples_count * sizeof(RadianceQuery));
	import_cuda_external_memory((void **)&radiance_query_addr_cuda, radiance_query_mem_cuda,
								radiance_query_buffer_pong.buffer_memory, max_samples_count * sizeof(RadianceQuery),
								getDefaultMemHandleType(), instance->vk_ctx.device);
	radiance_target_buffer_pong.create_external(&instance->vkb.ctx,
												VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
													VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
												VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
												max_samples_count * 3 * sizeof(float));
	import_cuda_external_memory((void **)&radiance_target_addr_cuda, radiance_target_mem_cuda,
								radiance_target_buffer_pong.buffer_memory, max_samples_count * 3 * sizeof(float),
								getDefaultMemHandleType(), instance->vk_ctx.device);
	sample_count_buffer.create_external(&instance->vkb.ctx,
										VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
											VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
										VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
										sizeof(uint32_t));
	import_cuda_external_memory((void **)&sample_count_addr_cuda, sample_count_mem_cuda,
								sample_count_buffer.buffer_memory, sizeof(uint32_t), getDefaultMemHandleType(),
								instance->vk_ctx.device);

	inference_query_buffer.create_external(&instance->vkb.ctx,
										   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
											   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
										   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
										   instance->width * instance->height * sizeof(RadianceQuery));

	import_cuda_external_memory(
		(void **)&inference_query_addr_cuda, inference_query_mem_cuda, inference_query_buffer.buffer_memory,
		instance->width * instance->height * sizeof(RadianceQuery), getDefaultMemHandleType(), instance->vk_ctx.device);

	inference_radiance_buffer.create_external(&instance->vkb.ctx,
											  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
												  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
											  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
											  instance->width * instance->height * 3 * sizeof(float));

	import_cuda_external_memory(
		(void **)&inference_radiance_addr_cuda, inference_radiance_mem_cuda, inference_radiance_buffer.buffer_memory,
		instance->width * instance->height * 3 * sizeof(float), getDefaultMemHandleType(), instance->vk_ctx.device);

	radiance_query_buffer_ping.create(&instance->vkb.ctx,
									  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
										  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
									  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
									  max_samples_count * sizeof(RadianceQuery));

	radiance_target_buffer_ping.create(&instance->vkb.ctx,
									   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
										   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
									   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
									   max_samples_count * 3 * sizeof(float));

	throughput_buffer.create(&instance->vkb.ctx,
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
								 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
							 instance->width * instance->height * 3 * sizeof(float));

	// ReSTIR buffers
	g_buffer.create("G-Buffer", &instance->vkb.ctx,
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
						VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
					instance->width * instance->height * sizeof(RestirGBufferData));

	temporal_reservoir_buffer.create("Temporal Reservoirs", &instance->vkb.ctx,
									 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
										 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
									 instance->width * instance->height * sizeof(RestirReservoir));

	passthrough_reservoir_buffer.create("Passthrough Buffer", &instance->vkb.ctx,
										VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
											VK_BUFFER_USAGE_TRANSFER_DST_BIT,
										VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
										instance->width * instance->height * sizeof(RestirReservoir));

	spatial_reservoir_buffer.create("Spatial Reservoirs", &instance->vkb.ctx,
									VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
										VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
									instance->width * instance->height * sizeof(RestirReservoir));

	desc.radiance_query_addr = radiance_query_buffer_ping.get_device_address();
	desc.radiance_target_addr = radiance_target_buffer_ping.get_device_address();
	desc.radiance_query_out_addr = radiance_query_buffer_pong.get_device_address();
	desc.radiance_target_out_addr = radiance_target_buffer_pong.get_device_address();
	desc.sample_count_addr = sample_count_buffer.get_device_address();
	desc.inference_radiance_addr = inference_radiance_buffer.get_device_address();
	desc.inference_query_addr = inference_query_buffer.get_device_address();
	desc.throughput_addr = throughput_buffer.get_device_address();
	// ReSTIR
	desc.g_buffer_addr = g_buffer.get_device_address();
	desc.temporal_reservoir_addr = temporal_reservoir_buffer.get_device_address();
	desc.spatial_reservoir_addr = spatial_reservoir_buffer.get_device_address();
	desc.passthrough_reservoir_addr = passthrough_reservoir_buffer.get_device_address();

	scene_desc_buffer.create("Scene Desc", &instance->vkb.ctx,
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneDesc), &desc,
							 true);
	CommandBuffer cmd(&instance->vk_ctx, true);
	vkCmdFillBuffer(cmd.handle, sample_count_buffer.handle, 0, sample_count_buffer.size, 0);
	cmd.submit();

	pc_ray.frame_num = 0;
	pc_ray.size_x = instance->width;
	pc_ray.size_y = instance->height;
	assert(instance->vkb.rg->settings.shader_inference == true);
	// For shader resource dependency inference, use this macro to register a buffer address to the rendergraph
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, &prim_lookup_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, radiance_query_addr, &radiance_query_buffer_ping, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, radiance_target_addr, &radiance_target_buffer_ping, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, radiance_query_out_addr, &radiance_query_buffer_pong,
								 instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, radiance_target_out_addr, &radiance_target_buffer_pong,
								 instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, sample_count_addr, &sample_count_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, inference_radiance_addr, &inference_radiance_buffer,
								 instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, throughput_addr, &throughput_buffer, instance->vkb.rg);
	// RESTIR
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, prim_info_addr, &prim_lookup_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, g_buffer_addr, &g_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, temporal_reservoir_addr, &temporal_reservoir_buffer,
								 instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, spatial_reservoir_addr, &spatial_reservoir_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, passthrough_reservoir_addr, &passthrough_reservoir_buffer,
								 instance->vkb.rg);
}

void NRC::render() {
	CommandBuffer cmd(&instance->vkb.ctx, /*start*/ true);
	pc_ray.num_lights = (int)lights.size();
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.total_light_area = total_light_area;
	pc_ray.light_triangle_count = total_light_triangle_cnt;
	pc_ray.dir_light_idx = lumen_scene->dir_light_idx;
	pc_ray.min_bounds = lumen_scene->m_dimensions.min;
	pc_ray.max_bounds = lumen_scene->m_dimensions.max;
	pc_ray.tile_offset = rand() % (TILE_SIZE * TILE_SIZE);
	pc_ray.size_y = instance->height;
	pc_ray.size_x = instance->width;
	pc_ray.total_frame_num = total_frame_num;
	pc_ray.do_spatiotemporal = do_spatiotemporal;
	pc_ray.random_num = rand() % UINT_MAX;

	const std::initializer_list<ResourceBinding> rt_bindings = {
		output_tex,
		scene_ubo_buffer,
		scene_desc_buffer,
	};

	// Temporal pass + path tracing
	instance->vkb.rg
		->add_rt("ReSTIR - Temporal Pass", {.shaders = {{"src/shaders/integrators/restir/di/temporal_pass.rgen"},
														{"src/shaders/ray.rmiss"},
														{"src/shaders/ray_shadow.rmiss"},
														{"src/shaders/ray.rchit"},
														{"src/shaders/ray.rahit"}},
											.dims = {instance->width, instance->height},
											.accel = instance->vkb.tlas.accel})
		.push_constants(&pc_ray)
		.zero(g_buffer)
		.zero(spatial_reservoir_buffer)
		.zero(temporal_reservoir_buffer, !do_spatiotemporal)
		.bind(rt_bindings)
		.bind(mesh_lights_buffer)
		.bind_texture_array(scene_textures)
		.bind_tlas(instance->vkb.tlas);
	// Spatial pass
	instance->vkb.rg
		->add_rt("ReSTIR - Spatial Pass", {.shaders = {{"src/shaders/integrators/restir/di/spatial_pass.rgen"},
													   {"src/shaders/ray.rmiss"},
													   {"src/shaders/ray_shadow.rmiss"},
													   {"src/shaders/ray.rchit"},
													   {"src/shaders/ray.rahit"}},
										   .dims = {instance->width, instance->height},
										   .accel = instance->vkb.tlas.accel})
		.push_constants(&pc_ray)
		.bind(rt_bindings)
		.bind(mesh_lights_buffer)
		.bind_texture_array(scene_textures)
		.bind_tlas(instance->vkb.tlas);

	// Collect samples
	for (int i = 0; i < COLLECTION_FRAME_COUNT; i++) {
		instance->vkb.rg
			->add_rt("NRC - Collection", {.shaders = {{"src/shaders/integrators/nrc/nrc_train.rgen"},
													  {"src/shaders/ray.rmiss"},
													  {"src/shaders/ray_shadow.rmiss"},
													  {"src/shaders/ray.rchit"},
													  {"src/shaders/ray.rahit"}},
										  .dims = {instance->width, instance->height},
										  .accel = instance->vkb.tlas.accel})
			.push_constants(&pc_ray)
			.zero(sample_count_buffer)
			.zero(radiance_target_buffer_ping)
			.zero(radiance_query_buffer_ping)
			.bind({
				output_tex,
				scene_ubo_buffer,
				scene_desc_buffer,
			})
			.bind(mesh_lights_buffer)
			.bind_texture_array(scene_textures)
			.bind_tlas(instance->vkb.tlas);
	}

	// Shuffle
	uint32_t wg_x = (max_samples_count + 1023) / 1024;
	uint32_t wg_y = 1;
	instance->vkb.rg
		->add_compute("NRC - Shuffle",
					  {.shader = Shader("src/shaders/integrators/nrc/shuffle.comp"), .dims = {wg_x, wg_y}})
		.push_constants(&pc_ray)
		.bind(scene_desc_buffer)
		.zero(radiance_query_buffer_pong)
		.zero(radiance_target_buffer_pong);

	instance->vkb.rg->run_and_submit(cmd);
	// Train
	const int NUM_TRAIN_STEPS = 4;
	uint32_t batch_size = max_samples_count / NUM_TRAIN_STEPS;
	uint32_t data_start_idx = 0;
	for (int i = 0; i < NUM_TRAIN_STEPS; i++) {
		LUMEN_INFO("Training step {}", i);
		neural_radiance_cache.train(cu_stream, radiance_query_addr_cuda + 14 * data_start_idx,
									radiance_target_addr_cuda + 3 * data_start_idx, batch_size, nullptr);
		data_start_idx += batch_size;
	}
	cudaStreamSynchronize(cu_stream);
	cudaDeviceSynchronize();

	cmd.begin();
	instance->vkb.rg
		->add_rt("NRC - Infer", {.shaders = {{"src/shaders/integrators/nrc/nrc_infer.rgen"},
											 {"src/shaders/ray.rmiss"},
											 {"src/shaders/ray_shadow.rmiss"},
											 {"src/shaders/ray.rchit"},
											 {"src/shaders/ray.rahit"}},
								 .dims = {instance->width, instance->height},
								 .accel = instance->vkb.tlas.accel})
		.push_constants(&pc_ray)
		.zero(sample_count_buffer)
		.zero(inference_radiance_buffer)
		.zero(inference_query_buffer)
		.bind({
			output_tex,
			scene_ubo_buffer,
			scene_desc_buffer,
		})
		.bind(mesh_lights_buffer)
		.bind_texture_array(scene_textures)
		.bind_tlas(instance->vkb.tlas);

	instance->vkb.rg->run_and_submit(cmd);

	if (total_frame_num > 0) {
		neural_radiance_cache.infer(cu_stream, inference_query_addr_cuda, instance->width * instance->height,
									inference_radiance_addr_cuda);
		cudaStreamSynchronize(cu_stream);
		cudaDeviceSynchronize();
	}
	cmd.begin();
	// Output
	instance->vkb.rg
		->add_rt("ReSTIR - Output", {.shaders = {{"src/shaders/integrators/nrc/composit.rgen"},
												 {"src/shaders/ray.rmiss"},
												 {"src/shaders/ray_shadow.rmiss"},
												 {"src/shaders/ray.rchit"},
												 {"src/shaders/ray.rahit"}},
									 .dims = {instance->width, instance->height},
									 .accel = instance->vkb.tlas.accel})
		.push_constants(&pc_ray)
		.bind(rt_bindings)
		.bind(mesh_lights_buffer)
		.bind_texture_array(scene_textures)
		.bind_tlas(instance->vkb.tlas);

	if (!do_spatiotemporal) {
		do_spatiotemporal = true;
	}
	instance->vkb.rg->run_and_submit(cmd);
}

bool NRC::update() {
	pc_ray.frame_num++;
	total_frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		pc_ray.frame_num = 0;
	}
	return updated;
}

void NRC::destroy() { Integrator::destroy(); }
