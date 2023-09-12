#include "LumenPCH.h"
#include "NRC.h"
const int COLLECTION_FRAME_COUNT = 1;
void NRC::init() {
	Integrator::init();
	SceneDesc desc;
	desc.vertex_addr = vertex_buffer.get_device_address();
	desc.index_addr = index_buffer.get_device_address();
	desc.normal_addr = normal_buffer.get_device_address();
	desc.uv_addr = uv_buffer.get_device_address();
	desc.material_addr = materials_buffer.get_device_address();
	desc.prim_info_addr = prim_lookup_buffer.get_device_address();

	max_samples_count = instance->width * instance->height * COLLECTION_FRAME_COUNT;

	radiance_query_buffer_pong.create_external(
		&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, max_samples_count * sizeof(RadianceQuery));
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
	radiance_query_buffer_ping.create(&instance->vkb.ctx,
									  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
										  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
									  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
									  max_samples_count * sizeof(RadianceQuery));

	import_cuda_external_memory((void **)&sample_count_addr_cuda, sample_count_mem_cuda,
								sample_count_buffer.buffer_memory, sizeof(uint32_t), getDefaultMemHandleType(),
								instance->vk_ctx.device);
	radiance_target_buffer_ping.create(&instance->vkb.ctx,
									   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
										   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
									   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
									   max_samples_count * 3 * sizeof(float));

	desc.radiance_query_addr = radiance_query_buffer_ping.get_device_address();
	desc.radiance_target_addr = radiance_target_buffer_ping.get_device_address();
	desc.sample_count_addr = sample_count_buffer.get_device_address();

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
	REGISTER_BUFFER_WITH_ADDRESS(SceneDesc, desc, sample_count_addr, &sample_count_buffer, instance->vkb.rg);
}

void NRC::render() {
	CommandBuffer cmd(&instance->vkb.ctx, /*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	pc_ray.num_lights = (int)lights.size();
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = config->path_length;
	pc_ray.sky_col = config->sky_col;
	pc_ray.total_light_area = total_light_area;
	pc_ray.light_triangle_count = total_light_triangle_cnt;
	pc_ray.dir_light_idx = lumen_scene->dir_light_idx;
	pc_ray.min_bounds = lumen_scene->m_dimensions.min;
	pc_ray.max_bounds = lumen_scene->m_dimensions.max;
	const int TILE_SIZE = 8;
	pc_ray.tile_offset = rand() % (TILE_SIZE * TILE_SIZE);
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
		.bind(radiance_query_buffer_ping)
		.bind(radiance_target_buffer_ping)
		.bind(radiance_query_buffer_pong)
		.bind(radiance_target_buffer_pong);
	instance->vkb.rg->run_and_submit(cmd);
	// Train
}

bool NRC::update() {
	pc_ray.frame_num++;
	bool updated = Integrator::update();
	if (updated) {
		pc_ray.frame_num = 0;
	}
	return updated;
}

void NRC::destroy() { Integrator::destroy(); }
