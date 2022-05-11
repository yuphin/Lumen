#include "LumenPCH.h"
#include "DDGI.h"
#include <random>
constexpr int IRRADIANCE_SIDE_LENGTH = 8;
constexpr int DEPTH_SIDE_LENGTH = 16;

void DDGI::init() {
	Integrator::init();

	// DDGI Resources
	{
		glm::vec3 min_pos = lumen_scene->m_dimensions.min;
		glm::vec3 max_pos = lumen_scene->m_dimensions.max;
		probe_distance = 0.75f;
		glm::vec3 diag = (max_pos - min_pos) * 1.1f;
		probe_counts = glm::ivec3(diag / probe_distance);
		probe_start_position = min_pos;
		glm::vec3 bbox_div_probes = diag / glm::vec3(probe_counts);
		max_distance = bbox_div_probes.length() * 1.5f;

		const int irradiance_width = (IRRADIANCE_SIDE_LENGTH + 2) * probe_counts.x * probe_counts.y + 2;
		const int irradiance_height = (IRRADIANCE_SIDE_LENGTH + 2) * probe_counts.z + 2;

		const int depth_width = (DEPTH_SIDE_LENGTH + 2) * probe_counts.x * probe_counts.y + 2;
		const int depth_height = (DEPTH_SIDE_LENGTH + 2) * probe_counts.z + 2;
		TextureSettings settings;
		// Irradiance and depth
		for (int i = 0; i < 2; i++) {
			settings.base_extent = { (uint32_t)irradiance_width , (uint32_t)irradiance_height, 1 };
			settings.format = VK_FORMAT_R16G16B16A16_SFLOAT;
			settings.usage_flags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			irr_texes[i].create_empty_texture(&instance->vkb.ctx, settings,
											  VK_IMAGE_LAYOUT_GENERAL, bilinear_sampler);
			settings.base_extent = { (uint32_t)depth_width , (uint32_t)depth_height, 1 };
			settings.format = VK_FORMAT_R16G16_SFLOAT;
			depth_texes[i].create_empty_texture(&instance->vkb.ctx, settings,
												VK_IMAGE_LAYOUT_GENERAL, bilinear_sampler);
		}

		// RT
		{
			auto num_probes = probe_counts.x * probe_counts.y * probe_counts.z;
			TextureSettings settings;
			settings.base_extent = { (uint32_t)rays_per_probe, (uint32_t)num_probes, 1 };
			settings.format = VK_FORMAT_R16G16B16A16_SFLOAT;
			settings.usage_flags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			rt.radiance_tex.create_empty_texture(&instance->vkb.ctx, settings,
												 VK_IMAGE_LAYOUT_GENERAL, nearest_sampler);
			rt.dir_depth_tex.create_empty_texture(&instance->vkb.ctx, settings,
												  VK_IMAGE_LAYOUT_GENERAL, nearest_sampler);
		}
		// DDGI Output
		settings.base_extent = { (uint32_t)instance->width, (uint32_t)instance->height, 1 };
		settings.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		settings.usage_flags =
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_SAMPLE_COUNT_1_BIT;
		output.tex.create_empty_texture(&instance->vkb.ctx, settings,
										VK_IMAGE_LAYOUT_GENERAL, bilinear_sampler);
	}
	g_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		instance->width * instance->height * sizeof(GBufferData)
	);

	direct_lighting_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
		instance->width * instance->height * sizeof(glm::vec3)
	);

	ddgi_ubo_buffer.create(
		&instance->vkb.ctx,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE, sizeof(DDGIUniforms));

	SceneDesc desc;
	desc.vertex_addr = vertex_buffer.get_device_address();
	desc.index_addr = index_buffer.get_device_address();
	desc.normal_addr = normal_buffer.get_device_address();
	desc.uv_addr = uv_buffer.get_device_address();
	desc.material_addr = materials_buffer.get_device_address();
	desc.prim_info_addr = prim_lookup_buffer.get_device_address();
	desc.direct_lighting_addr = direct_lighting_buffer.get_device_address();



	desc.g_buffer_addr = g_buffer.get_device_address();
	scene_desc_buffer.create(&instance->vkb.ctx,
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
							 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							 VK_SHARING_MODE_EXCLUSIVE, sizeof(SceneDesc),
							 &desc, true);

	// Samplers
	{
		VkSamplerCreateInfo sampler_ci = vk::sampler_create_info();
		sampler_ci.minFilter = VK_FILTER_LINEAR;
		sampler_ci.magFilter = VK_FILTER_LINEAR;
		sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler_ci.maxLod = FLT_MAX;
		vk::check(vkCreateSampler(instance->vkb.ctx.device, &sampler_ci, nullptr,
				  &bilinear_sampler),
				  "Could not create image sampler");
		sampler_ci.minFilter = VK_FILTER_NEAREST;
		sampler_ci.magFilter = VK_FILTER_NEAREST;
		vk::check(vkCreateSampler(instance->vkb.ctx.device, &sampler_ci, nullptr,
				  &nearest_sampler),
				  "Could not create image sampler");
	}

	update_uniform_buffers();
	update_ddgi_uniforms();

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
}

void DDGI::render() {
	CommandBuffer cmd(&instance->vkb.ctx, /*start*/ true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	VkClearValue clear_color = { 0.25f, 0.25f, 0.25f, 1.0f };
	VkClearValue clear_depth = { 1.0f, 0 };
	VkViewport viewport = vk::viewport((float)instance->width, (float)instance->height, 0.0f, 1.0f);
	VkClearValue clear_values[] = { clear_color, clear_depth };
	pc_ray.time = rand() % UINT_MAX;
	pc_ray.max_depth = lumen_scene->config.path_length;
	pc_ray.sky_col = lumen_scene->config.sky_col;
	pc_ray.first_frame = first_frame;
	bool ping_pong = bool(frame_idx % 2); // ping_pong = read
	// Generate random orientation for probes
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<> dis(-1.0, 1.0);
		glm::vec4 rands(0.5 * dis(gen) + 0.5, dis(gen), dis(gen), dis(gen));
		pc_ray.probe_rotation = glm::mat4_cast(
			glm::angleAxis(2.0f * glm::pi<float>() * rands.x, glm::normalize(glm::vec3(rands.y, rands.z, rands.w)))
		);
	}
	// Clear color image
	VkClearColorValue clear_color_val = { 0,0,0,1 };
	VkImageSubresourceRange isr;
	isr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	isr.baseMipLevel = 0;
	isr.levelCount = 1;
	isr.baseArrayLayer = 0;
	isr.layerCount = 1;
	transition_image_layout(cmd.handle, output_tex.img, VK_IMAGE_LAYOUT_GENERAL,
							VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
	vkCmdClearColorImage(cmd.handle, output_tex.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color_val, 1, &isr);
	transition_image_layout(cmd.handle, output_tex.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT);
	if (first_frame) {

		transition_image_layout(cmd.handle, irr_texes[ping_pong].img,
								VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		transition_image_layout(cmd.handle, depth_texes[ping_pong].img,
								VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	// Trace Primary rays and fill G buffer
	{
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
						  vis_pipeline->handle);
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vis_pipeline->pipeline_layout,
			0, 1, &scene_desc_set, 0, nullptr);
		vkCmdPushConstants(cmd.handle, vis_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_RAYGEN_BIT_KHR |
						   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
						   VK_SHADER_STAGE_MISS_BIT_KHR,
						   0, sizeof(PushConstantRay), &pc_ray);
		auto& regions = vis_pipeline->get_rt_regions();
		vkCmdTraceRaysKHR(cmd.handle, &regions[0], &regions[1], &regions[2], &regions[3], instance->width, instance->height, 1);
		auto barrier = buffer_barrier(g_buffer.handle,
									  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
									  VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
		vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &barrier, 0, 0);
	}
	// Trace rays from probes
	{
		std::array<VkImageMemoryBarrier, 2> image_barriers = {
			image_barrier(rt.radiance_tex.img, VK_ACCESS_SHADER_READ_BIT,
				VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT),
			image_barrier(rt.dir_depth_tex.img, VK_ACCESS_SHADER_READ_BIT,
				VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT)
		};
		vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
							 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, image_barriers.size(), image_barriers.data());

		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
						  probe_trace_pipeline->handle);
		std::array<VkDescriptorSet, 4> descriptor_sets = {
				scene_desc_set,
				ddgi_uniform.desc_set,
				rt.write_desc_set,
				rt.read_desc_set
		};
		vkCmdBindDescriptorSets(
			cmd.handle, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, probe_trace_pipeline->pipeline_layout,
			0, descriptor_sets.size(), descriptor_sets.data(), 0, nullptr);
		vkCmdPushConstants(cmd.handle, probe_trace_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_RAYGEN_BIT_KHR |
						   VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
						   VK_SHADER_STAGE_MISS_BIT_KHR,
						   0, sizeof(PushConstantRay), &pc_ray);
		auto& regions = probe_trace_pipeline->get_rt_regions();
		int grid_size = probe_counts.x * probe_counts.y * probe_counts.z;
		vkCmdTraceRaysKHR(cmd.handle, &regions[0], &regions[1], &regions[2], &regions[3], rays_per_probe, grid_size, 1);
		image_barriers = {
			image_barrier(rt.radiance_tex.img, VK_ACCESS_SHADER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
			image_barrier(rt.dir_depth_tex.img, VK_ACCESS_SHADER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,
				VK_IMAGE_LAYOUT_GENERAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT)
		};
		vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
							 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, image_barriers.size(), image_barriers.data());
	
	}
	// Update probes & borders
	{
		auto update_probe = [&](bool is_irr) {
			Pipeline* pipeline;
			if (is_irr) {
				pipeline = irr_update_pipeline.get();
			} else {
				pipeline = depth_update_pipeline.get();
			}
			vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->handle);
			std::array<VkDescriptorSet, 5> descriptor_sets = {
				scene_desc_set,
				ddgi.write_desc_sets[!ping_pong],
				ddgi.read_desc_sets[ping_pong],
				ddgi_uniform.desc_set,
				rt.read_desc_set
			};
			vkCmdPushConstants(cmd.handle, pipeline->pipeline_layout,
							   VK_SHADER_STAGE_COMPUTE_BIT,
							   0, sizeof(PushConstantRay), &pc_ray);
			auto wg_x = probe_counts.x * probe_counts.y;
			auto wg_y = probe_counts.z;
			vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
									pipeline->pipeline_layout, 0, descriptor_sets.size()
									,
									descriptor_sets.data(), 0, nullptr);
			vkCmdDispatch(cmd.handle, wg_x, wg_y, 1);
		};

		// Probes
		{
			std::array<VkImageMemoryBarrier, 2> image_barriers = {
						image_barrier(irr_texes[!ping_pong].img, VK_ACCESS_SHADER_READ_BIT,
							VK_ACCESS_SHADER_WRITE_BIT,
							VK_IMAGE_LAYOUT_UNDEFINED,
							VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT),
						image_barrier(depth_texes[!ping_pong].img, VK_ACCESS_SHADER_READ_BIT,
							VK_ACCESS_SHADER_WRITE_BIT,
							VK_IMAGE_LAYOUT_UNDEFINED,
							VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT)
			};
			vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
								 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
								 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, image_barriers.size(), image_barriers.data());
			update_probe(true);
			update_probe(false);

			image_barriers = {
				   image_barrier(irr_texes[!ping_pong].img, VK_ACCESS_SHADER_WRITE_BIT,
					   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
					   VK_IMAGE_LAYOUT_GENERAL,
					   VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT),
				   image_barrier(depth_texes[!ping_pong].img, VK_ACCESS_SHADER_WRITE_BIT,
					   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
					   VK_IMAGE_LAYOUT_GENERAL,
					   VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT)
			};
			vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
								 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
								 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, image_barriers.size(), image_barriers.data());
		}
		// Borders
		{
			vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, border_update_pipeline->handle);
			std::array<VkDescriptorSet, 2> descriptor_sets = {
				ddgi.write_desc_sets[!ping_pong],
				ddgi_uniform.desc_set
			};
			vkCmdPushConstants(cmd.handle, border_update_pipeline->pipeline_layout,
							   VK_SHADER_STAGE_COMPUTE_BIT,
							   0, sizeof(PushConstantRay), &pc_ray);
			// 13 WGs process 4 probes (wg = 32 threads)
			auto wg_x = (probe_counts.x * probe_counts.y * probe_counts.z + 3) * 13 / 4;

			vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
									border_update_pipeline->pipeline_layout, 0, descriptor_sets.size(),
									descriptor_sets.data(), 0, nullptr);
			vkCmdDispatch(cmd.handle, wg_x, 1, 1);
			std::array<VkImageMemoryBarrier, 2> image_barriers = {
			   image_barrier(irr_texes[!ping_pong].img, VK_ACCESS_SHADER_WRITE_BIT,
				   VK_ACCESS_SHADER_READ_BIT,
				   VK_IMAGE_LAYOUT_GENERAL,
				   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT),
			   image_barrier(depth_texes[!ping_pong].img, VK_ACCESS_SHADER_WRITE_BIT,
				   VK_ACCESS_SHADER_READ_BIT,
				   VK_IMAGE_LAYOUT_GENERAL,
				   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT)
			};
			vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
								 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
								 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, image_barriers.size(), image_barriers.data());
		}
	}
	// Sample probes & output into texture
	{
		std::array<VkImageMemoryBarrier, 1> image_barriers = {
			image_barrier(output.tex.img, VK_ACCESS_SHADER_READ_BIT,
				VK_ACCESS_SHADER_WRITE_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT)
		};
		vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, image_barriers.size(), image_barriers.data());
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, sample_pipeline->handle);
		vkCmdPushConstants(cmd.handle, sample_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_COMPUTE_BIT,
						   0, sizeof(PushConstantRay), &pc_ray);
		std::array<VkDescriptorSet, 4> descriptor_sets = {
			scene_desc_set,
			output.write_desc_set,
			ddgi.read_desc_sets[ping_pong],
			ddgi_uniform.desc_set
		};
		vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
								sample_pipeline->pipeline_layout, 0, descriptor_sets.size(),
								descriptor_sets.data(), 0, nullptr);

		auto wg_x = (instance->width + 31) / 32;
		auto wg_y = (instance->height + 31) / 32;
		vkCmdDispatch(cmd.handle, wg_x, wg_y, 1);
		image_barriers = {
		image_barrier(output.tex.img, VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT)
		};
		vkCmdPipelineBarrier(cmd.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, image_barriers.size(), image_barriers.data());
	}

	// Output
	{
		vkCmdBindPipeline(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE, out_pipeline->handle);
		vkCmdPushConstants(cmd.handle, out_pipeline->pipeline_layout,
						   VK_SHADER_STAGE_COMPUTE_BIT,
						   0, sizeof(PushConstantRay), &pc_ray);
		std::array<VkDescriptorSet, 2> descriptor_sets = {
			scene_desc_set,
			output.read_desc_set
		};
		vkCmdBindDescriptorSets(cmd.handle, VK_PIPELINE_BIND_POINT_COMPUTE,
								out_pipeline->pipeline_layout, 0, descriptor_sets.size(),
								descriptor_sets.data(), 0, nullptr);

		auto wg_x = (instance->width + 31) / 32;
		auto wg_y = (instance->height + 31) / 32;
		vkCmdDispatch(cmd.handle, wg_x, wg_y, 1);
	}
	vkCmdFillBuffer(cmd.handle, g_buffer.handle, 0, g_buffer.size, 0);
	cmd.submit();
	frame_idx++;
	first_frame = false;
}

bool DDGI::update() {
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

void DDGI::create_offscreen_resources() {
	// Create offscreen image for output
	TextureSettings settings;
	settings.usage_flags =
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	settings.base_extent = { (uint32_t)instance->width, (uint32_t)instance->height, 1 };
	settings.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	output_tex.create_empty_texture(&instance->vkb.ctx, settings,
									VK_IMAGE_LAYOUT_GENERAL);
	CommandBuffer cmd(&instance->vkb.ctx, true);
	transition_image_layout(cmd.handle, output_tex.img,
							VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	cmd.submit();
}

void DDGI::create_descriptors() {
	constexpr int TLAS_BINDING = 0;
	constexpr int IMAGE_BINDING = 1;
	constexpr int INSTANCE_BINDING = 2;
	constexpr int UNIFORM_BUFFER_BINDING = 3;
	constexpr int SCENE_DESC_BINDING = 4;
	constexpr int TEXTURES_BINDING = 5;
	constexpr int LIGHTS_BINDING = 6;

	auto num_textures = textures.size();
	std::vector<VkDescriptorPoolSize> pool_sizes = {
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
								 num_textures + 5),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
								 1),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 + 5) };
	auto descriptor_pool_ci =
		vk::descriptor_pool_CI(pool_sizes.size(), pool_sizes.data(),
							   10);

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
			VK_SHADER_STAGE_ALL,
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
			  nullptr, &scene_desc_set_layout),
			  "Failed to create escriptor set layout");
	VkDescriptorSetAllocateInfo set_allocate_info{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	set_allocate_info.descriptorPool = desc_pool;
	set_allocate_info.descriptorSetCount = 1;
	set_allocate_info.pSetLayouts = &scene_desc_set_layout;
	vkAllocateDescriptorSets(instance->vkb.ctx.device, &set_allocate_info, &scene_desc_set);

	// Update descriptors
	VkAccelerationStructureKHR tlas = instance->vkb.tlas.accel;
	VkWriteDescriptorSetAccelerationStructureKHR desc_as_info{
		VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
	desc_as_info.accelerationStructureCount = 1;
	desc_as_info.pAccelerationStructures = &tlas;

	// TODO: Abstraction
	std::vector<VkWriteDescriptorSet> writes{
		vk::write_descriptor_set(scene_desc_set,
								 VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
								 TLAS_BINDING, &desc_as_info),
		vk::write_descriptor_set(scene_desc_set, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
								 IMAGE_BINDING,
								 &output_tex.descriptor_image_info),
		vk::write_descriptor_set(scene_desc_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
								 INSTANCE_BINDING,
								 &prim_lookup_buffer.descriptor),
	   vk::write_descriptor_set(scene_desc_set,
								VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
								UNIFORM_BUFFER_BINDING, &scene_ubo_buffer.descriptor),
	   vk::write_descriptor_set(scene_desc_set,
								VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
								SCENE_DESC_BINDING, &scene_desc_buffer.descriptor) };
	std::vector<VkDescriptorImageInfo> image_infos;
	for (auto& tex : textures) {
		image_infos.push_back(tex.descriptor_image_info);
	}
	writes.push_back(vk::write_descriptor_set(scene_desc_set,
					 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, TEXTURES_BINDING,
					 image_infos.data(), (uint32_t)image_infos.size()));
	if (lights.size()) {
		writes.push_back(vk::write_descriptor_set(
			scene_desc_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, LIGHTS_BINDING,
			&mesh_lights_buffer.descriptor));
	}
	vkUpdateDescriptorSets(
		instance->vkb.ctx.device, static_cast<uint32_t>(writes.size()),
		writes.data(), 0, nullptr);


	// DDGI
	{
		// UBO
		{
			std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
		vk::descriptor_set_layout_binding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_SHADER_STAGE_ALL,
				0)
			};
			auto set_layout_ci = vk::descriptor_set_layout_CI(
				set_layout_bindings.data(), set_layout_bindings.size());
			vk::check(vkCreateDescriptorSetLayout(instance->vkb.ctx.device, &set_layout_ci,
					  nullptr, &ddgi_uniform.desc_layout),
					  "Failed to create descriptor set layout");

			auto set_allocate_info =
				vk::descriptor_set_allocate_info(desc_pool, &ddgi_uniform.desc_layout, 1);
			vk::check(vkAllocateDescriptorSets(instance->vkb.ctx.device, &set_allocate_info,
					  &ddgi_uniform.desc_set),
					  "Failed to allocate descriptor sets");
		}
		// DDGI IRR & Depth
		{
			constexpr int DDGI_IRR_BINDING = 0;
			constexpr int DDGI_DEPTH_BINDING = 1;
			std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
								vk::descriptor_set_layout_binding(
							VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
						VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
						DDGI_IRR_BINDING),
						vk::descriptor_set_layout_binding(
							VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
						VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
							DDGI_DEPTH_BINDING),
			};
			auto set_layout_ci = vk::descriptor_set_layout_CI(
				set_layout_bindings.data(), set_layout_bindings.size());
			vk::check(vkCreateDescriptorSetLayout(instance->vkb.ctx.device, &set_layout_ci,
					  nullptr, &ddgi.write_desc_layout),
					  "Failed to create descriptor set layout");
			set_layout_bindings = {
							vk::descriptor_set_layout_binding(
							VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT ,
						DDGI_IRR_BINDING),
					vk::descriptor_set_layout_binding(
							VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
						DDGI_DEPTH_BINDING),
			};
			set_layout_ci = vk::descriptor_set_layout_CI(
				set_layout_bindings.data(), set_layout_bindings.size());
			vk::check(vkCreateDescriptorSetLayout(instance->vkb.ctx.device, &set_layout_ci,
					  nullptr, &ddgi.read_desc_layout),
					  "Failed to create descriptor set layout");
			VkDescriptorSetLayout layouts[2] = { ddgi.write_desc_layout, ddgi.read_desc_layout };
			for (int i = 0; i < 2; i++) {
				VkDescriptorSet* sets[2] = { &ddgi.write_desc_sets[i], &ddgi.read_desc_sets[i] };
				for (int j = 0; j < 2; j++) {
					auto set_allocate_info =
						vk::descriptor_set_allocate_info(desc_pool, &layouts[j], 1);
					vk::check(vkAllocateDescriptorSets(instance->vkb.ctx.device, &set_allocate_info,
							  sets[j]),
							  "Failed to allocate descriptor sets");
				}
			}
		}
		// RT
		{
			std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
									vk::descriptor_set_layout_binding(
								VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
							VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
								0),
							vk::descriptor_set_layout_binding(
								VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
							VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
								1),
			};

			auto set_layout_ci = vk::descriptor_set_layout_CI(
				set_layout_bindings.data(), set_layout_bindings.size());
			vk::check(vkCreateDescriptorSetLayout(instance->vkb.ctx.device, &set_layout_ci,
					  nullptr, &rt.write_desc_layout),
					  "Failed to create descriptor set layout");


			set_layout_bindings = {
					vk::descriptor_set_layout_binding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
				0),
			vk::descriptor_set_layout_binding(
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
				1),
			};
			set_layout_ci = vk::descriptor_set_layout_CI(
				set_layout_bindings.data(), set_layout_bindings.size());
			vk::check(vkCreateDescriptorSetLayout(instance->vkb.ctx.device, &set_layout_ci,
					  nullptr, &rt.read_desc_layout),
					  "Failed to create descriptor set layout");
			VkDescriptorSetLayout layouts[2] = { rt.write_desc_layout, rt.read_desc_layout};
			VkDescriptorSet* sets[2] = { &rt.write_desc_set, &rt.read_desc_set };
			for (int i = 0; i < 2; i++) {
				auto set_allocate_info =
					vk::descriptor_set_allocate_info(desc_pool, &layouts[i], 1);
				vk::check(vkAllocateDescriptorSets(instance->vkb.ctx.device, &set_allocate_info,
						  sets[i]),
						  "Failed to allocate descriptor sets");
			}
		}

		// DDGI Output
		{
			std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
								vk::descriptor_set_layout_binding(
							VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
						VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
							0)
			};
			auto set_layout_ci = vk::descriptor_set_layout_CI(
				set_layout_bindings.data(), set_layout_bindings.size());
			vk::check(vkCreateDescriptorSetLayout(instance->vkb.ctx.device, &set_layout_ci,
					  nullptr, &output.write_desc_layout),
					  "Failed to create descriptor set layout");

			set_layout_bindings = {
					vk::descriptor_set_layout_binding(
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
				0),
			};
			set_layout_ci = vk::descriptor_set_layout_CI(
				set_layout_bindings.data(), set_layout_bindings.size());
			vk::check(vkCreateDescriptorSetLayout(instance->vkb.ctx.device, &set_layout_ci,
					  nullptr, &output.read_desc_layout),
					  "Failed to create descriptor set layout");
			VkDescriptorSetLayout layouts[2] = { output.write_desc_layout, output.read_desc_layout };
			VkDescriptorSet* sets[2] = { &output.write_desc_set, &output.read_desc_set };
			for (int i = 0; i < 2; i++) {
				auto set_allocate_info =
					vk::descriptor_set_allocate_info(desc_pool, &layouts[i], 1);
				vk::check(vkAllocateDescriptorSets(instance->vkb.ctx.device, &set_allocate_info,
						  sets[i]),
						  "Failed to allocate descriptor sets");
			}
		
		}
	}

	// Update
	{
		// UBO
		{
			VkWriteDescriptorSet wds = vk::write_descriptor_set(
				ddgi_uniform.desc_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0, &ddgi_ubo_buffer.descriptor);			
			vkUpdateDescriptorSets(
				instance->vkb.ctx.device, 1,
				&wds, 0, nullptr);
		}
		// RT Write
		{
			std::vector<VkWriteDescriptorSet> writes = {
								vk::write_descriptor_set(
									rt.write_desc_set, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
									0, &rt.radiance_tex.descriptor_image_info),
								vk::write_descriptor_set(
									rt.write_desc_set, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
									1, &rt.dir_depth_tex.descriptor_image_info),
			};
			vkUpdateDescriptorSets(
				instance->vkb.ctx.device, static_cast<uint32_t>(writes.size()),
				writes.data(), 0, nullptr);
		}
		// RT Read
		{
			std::vector<VkDescriptorImageInfo> read_infos = {
								rt.radiance_tex.create_descriptor_info(nearest_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
								rt.dir_depth_tex.create_descriptor_info(nearest_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			};
			std::vector<VkWriteDescriptorSet> writes = {
					vk::write_descriptor_set(
						rt.read_desc_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						0, &read_infos[0]),
					vk::write_descriptor_set(
						rt.read_desc_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
						1, &read_infos[1]),
			};
			vkUpdateDescriptorSets(
				instance->vkb.ctx.device, static_cast<uint32_t>(writes.size()),
				writes.data(), 0, nullptr);
		}
		// Probe Writes
		{
			std::vector<VkWriteDescriptorSet> writes;
			std::vector<VkDescriptorImageInfo> infos = {
				irr_texes[0].create_descriptor_info(0, VK_IMAGE_LAYOUT_GENERAL),
				depth_texes[0].create_descriptor_info(0, VK_IMAGE_LAYOUT_GENERAL),
				irr_texes[1].create_descriptor_info(0, VK_IMAGE_LAYOUT_GENERAL),
				depth_texes[1].create_descriptor_info(0, VK_IMAGE_LAYOUT_GENERAL)
			};
			for (int i = 0; i < 2; i++) {
				writes.push_back(
					vk::write_descriptor_set(
					ddgi.write_desc_sets[i], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					0, &infos[2 * i])
				);
				writes.push_back(
					vk::write_descriptor_set(
					ddgi.write_desc_sets[i], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					1, &infos[2 * i + 1])
				);
			}
			vkUpdateDescriptorSets(
				instance->vkb.ctx.device, static_cast<uint32_t>(writes.size()),
				writes.data(), 0, nullptr);
		}

		// Probe Reads
		{
			std::vector<VkDescriptorImageInfo> infos = {
						irr_texes[0].create_descriptor_info(bilinear_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
						depth_texes[0].create_descriptor_info(bilinear_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
						irr_texes[1].create_descriptor_info(bilinear_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
						depth_texes[1].create_descriptor_info(bilinear_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			};
			std::vector<VkWriteDescriptorSet> writes;
			for (int i = 0; i < 2; i++) {
				writes.push_back(
					vk::write_descriptor_set(
					ddgi.read_desc_sets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					0, &infos[2 * i])
				);
				writes.push_back(
					vk::write_descriptor_set(
					ddgi.read_desc_sets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					1, &infos[2 * i + 1])
				);
			}
			vkUpdateDescriptorSets(
				instance->vkb.ctx.device, static_cast<uint32_t>(writes.size()),
				writes.data(), 0, nullptr);
		}

	}
	// Output
	auto wdi = output.tex.create_descriptor_info(0, VK_IMAGE_LAYOUT_GENERAL);
	VkWriteDescriptorSet write_data_w = vk::write_descriptor_set(
		output.write_desc_set, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
		0, &wdi);
	auto rdi = output.tex.create_descriptor_info(
		bilinear_sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	VkWriteDescriptorSet write_data_r = vk::write_descriptor_set(
		output.read_desc_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		0, &rdi);
	vkUpdateDescriptorSets(instance->vkb.ctx.device, 1, &write_data_w, 0, nullptr);
	vkUpdateDescriptorSets(instance->vkb.ctx.device, 1, &write_data_r, 0, nullptr);
}


void DDGI::create_blas() {
	std::vector<BlasInput> blas_inputs;
	auto vertex_address =
		get_device_address(instance->vkb.ctx.device, vertex_buffer.handle);
	auto idx_address = get_device_address(instance->vkb.ctx.device, index_buffer.handle);
	for (auto& prim_mesh : lumen_scene->prim_meshes) {
		BlasInput geo = to_vk_geometry(prim_mesh, vertex_address, idx_address);
		blas_inputs.push_back({ geo });
	}
	instance->vkb.build_blas(blas_inputs,
							 VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
}

void DDGI::create_tlas() {
	std::vector<VkAccelerationStructureInstanceKHR> tlas;
	float total_light_triangle_area = 0.0f;
	//int light_triangle_cnt = 0;
	const auto& indices = lumen_scene->indices;
	const auto& vertices = lumen_scene->positions;
	for (const auto& pm : lumen_scene->prim_meshes) {
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
			const auto& pm = lumen_scene->prim_meshes[l.prim_mesh_idx];
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
			//light_triangle_cnt += l.num_triangles;
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

void DDGI::create_rt_pipelines() {
	enum StageIndices {
		Raygen,
		CMiss,
		AMiss,
		ClosestHit,
		AnyHit,
		ShaderGroupCount
	};
	RTPipelineSettings settings;

	std::vector<Shader> shaders = {
						{"src/shaders/integrators/ddgi/primary_rays.rgen"},
						{"src/shaders/integrators/ddgi/trace.rgen"},
						{"src/shaders/ray.rmiss"},
						{"src/shaders/ray_shadow.rmiss"},
						{"src/shaders/ray.rchit"},
						{"src/shaders/ray.rahit"}
	};
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
	stage.module = shaders[2].create_vk_shader_module(instance->vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[CMiss] = stage;

	stage.module = shaders[3].create_vk_shader_module(instance->vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	stages[AMiss] = stage;
	// Hit Group - Closest Hit
	stage.module = shaders[4].create_vk_shader_module(instance->vkb.ctx.device);
	stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	stages[ClosestHit] = stage;
	// Hit Group - Any hit
	stage.module = shaders[5].create_vk_shader_module(instance->vkb.ctx.device);
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
	settings.desc_layouts = { scene_desc_set_layout };
	settings.stages = stages;
	settings.shaders = { shaders[0], shaders[2], shaders[3], shaders[4], shaders[5] };
	vis_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	probe_trace_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	vis_pipeline->create_rt_pipeline(settings);
	vkDestroyShaderModule(instance->vkb.ctx.device, stages[Raygen].module, nullptr);
	stages[Raygen].module = shaders[1].create_vk_shader_module(instance->vkb.ctx.device);
	settings.stages = stages;
	settings.shaders[0] = shaders[1];
	settings.desc_layouts = { scene_desc_set_layout, ddgi_uniform.desc_layout, rt.write_desc_layout, rt.read_desc_layout };
	probe_trace_pipeline->create_rt_pipeline(settings);
	for (auto& s : settings.stages) {
		vkDestroyShaderModule(instance->vkb.ctx.device, s.module, nullptr);
	}
}

void DDGI::create_compute_pipelines() {
	depth_update_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	irr_update_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	border_update_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	sample_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	out_pipeline = std::make_unique<Pipeline>(instance->vkb.ctx.device);
	std::vector<Shader> shaders = {
		{"src/shaders/integrators/ddgi/update_depth.comp"},
		{"src/shaders/integrators/ddgi/update_irradiance.comp"},
		{"src/shaders/integrators/ddgi/update_borders.comp"},
		{"src/shaders/integrators/ddgi/sample.comp"},
		{"src/shaders/integrators/ddgi/out.comp"},
	};
	for (auto& shader : shaders) {
		shader.compile();
	}
	ComputePipelineSettings settings;
	std::vector<VkDescriptorSetLayout> layouts = {
		scene_desc_set_layout,
		ddgi.write_desc_layout,
		ddgi.read_desc_layout,
		ddgi_uniform.desc_layout,
		rt.read_desc_layout,
	};
	settings.desc_set_layouts = layouts.data();
	settings.desc_set_layout_cnt = layouts.size();
	settings.push_const_size = sizeof(PushConstantRay);
	settings.shader = shaders[0];
	depth_update_pipeline->create_compute_pipeline(settings);
	settings.shader = shaders[1];
	irr_update_pipeline->create_compute_pipeline(settings);
	layouts = {
		ddgi.write_desc_layout,
		ddgi_uniform.desc_layout
	};
	settings.shader = shaders[2];
	settings.desc_set_layouts = layouts.data();
	settings.desc_set_layout_cnt = layouts.size();
	border_update_pipeline->create_compute_pipeline(settings);
	layouts = {
		scene_desc_set_layout,
		output.write_desc_layout,
		ddgi.read_desc_layout,
		ddgi_uniform.desc_layout,
	};
	settings.shader = shaders[3];
	settings.desc_set_layouts = layouts.data();
	settings.desc_set_layout_cnt = layouts.size();
	sample_pipeline->create_compute_pipeline(settings);
	layouts = {
		scene_desc_set_layout,
		output.read_desc_layout,
	};
	settings.shader = shaders[4];
	settings.desc_set_layouts = layouts.data();
	settings.desc_set_layout_cnt = layouts.size();
	out_pipeline->create_compute_pipeline(settings);
}

void DDGI::update_ddgi_uniforms() {
	ddgi_ubo.probe_counts = probe_counts;
	ddgi_ubo.hysteresis = hysteresis;
	ddgi_ubo.probe_start_position = probe_start_position;
	ddgi_ubo.probe_step = probe_distance;
	ddgi_ubo.rays_per_probe = rays_per_probe;
	ddgi_ubo.max_distance = max_distance;
	ddgi_ubo.depth_sharpness = depth_sharpness;
	ddgi_ubo.normal_bias = normal_bias;
	ddgi_ubo.view_bias = view_bias;
	ddgi_ubo.irradiance_width = irr_texes[0].base_extent.width;
	ddgi_ubo.irradiance_height = irr_texes[0].base_extent.height;
	ddgi_ubo.depth_width = depth_texes[0].base_extent.width;
	ddgi_ubo.depth_height = depth_texes[0].base_extent.height;

	memcpy(ddgi_ubo_buffer.data, &ddgi_ubo, sizeof(ddgi_ubo));
}

void DDGI::destroy() {
	const auto device = instance->vkb.ctx.device;
	Integrator::destroy();
	vis_pipeline->cleanup();
	std::vector<Buffer*> buffer_list = {
	&g_buffer,
	};
	for (auto b : buffer_list) {
		b->destroy();
	}
	vkDestroyDescriptorSetLayout(device, scene_desc_set_layout, nullptr);
	vkDestroyDescriptorPool(device, desc_pool, nullptr);
}

void DDGI::reload() {
	vis_pipeline->reload();
}
