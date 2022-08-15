#include "LumenPCH.h"
#include <regex>
#include <stb_image.h>
#define TINYEXR_IMPLEMENTATION
#include <zlib.h>
#include <tinyexr.h>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include "RayTracer.h"

RayTracer* RayTracer::instance = nullptr;
static void fb_resize_callback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<RayTracer*>(glfwGetWindowUserPointer(window));
	app->resized = true;
}

RayTracer::RayTracer(int width, int height, bool debug, int argc, char* argv[]) : LumenInstance(width, height, debug) {
	this->instance = this;
	parse_args(argc, argv);
}

void RayTracer::init(Window* window) {
	srand((uint32_t)time(NULL));
	this->window = window;
	vkb.ctx.window_ptr = window->get_window_ptr();
	glfwSetFramebufferSizeCallback(vkb.ctx.window_ptr, fb_resize_callback);
	// Init with ray tracing extensions
	vkb.add_device_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
	vkb.add_device_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
	vkb.add_device_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	vkb.add_device_extension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
	vkb.add_device_extension(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);
	vkb.add_device_extension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
	vkb.add_device_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
	vkb.add_device_extension(VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME);

	vkb.create_instance();
	if (vkb.enable_validation_layers) {
		vkb.setup_debug_messenger();
	}
	vkb.create_surface();
	vkb.pick_physical_device();
	vkb.create_logical_device();
	vkb.create_swapchain();
	vkb.create_command_pool();
	vkb.create_command_buffers();
	vkb.create_sync_primitives();
	initialized = true;

	scene.load_scene(scene_name);
	vkb.rg->settings.shader_inference = true;
	switch (scene.config.integrator_type) {
		case IntegratorType::Path:
			integrator = std::make_unique<Path>(this, &scene);
			break;
		case IntegratorType::BDPT:
			integrator = std::make_unique<BDPT>(this, &scene);
			break;
		case IntegratorType::SPPM:
			integrator = std::make_unique<SPPM>(this, &scene);
			break;
			/*	case IntegratorType::VCM:
					integrator = std::make_unique<VCM>(this, &scene);
					break;
				case IntegratorType::PSSMLT:
					integrator = std::make_unique<PSSMLT>(this, &scene);
					break;
				case IntegratorType::SMLT:
					integrator = std::make_unique<SMLT>(this, &scene);
					break;
				case IntegratorType::VCMMLT:
					integrator = std::make_unique<VCMMLT>(this, &scene);
					break;
				case IntegratorType::ReSTIR:
					integrator = std::make_unique<ReSTIR>(this, &scene);
					break;
				case IntegratorType::ReSTIRGI:
					integrator = std::make_unique<ReSTIRGI>(this, &scene);
					break;
				case IntegratorType::DDGI:
					integrator = std::make_unique<DDGI>(this, &scene);
					break;
				case IntegratorType::ReSTIRPT:
					integrator = std::make_unique<ReSTIRPT>(this, &scene);
					break;
				default:
					break;*/
	}
	integrator->init();
	init_resources();
	create_post_descriptor();
	update_post_desc_set();
	create_post_pipeline();
	create_compute_pipelines();
	// init_imgui();
	VkPhysicalDeviceMemoryProperties2 props = {};
	props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
	VkPhysicalDeviceMemoryBudgetPropertiesEXT budget_props = {};
	budget_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
	props.pNext = &budget_props;
	vkGetPhysicalDeviceMemoryProperties2(vk_ctx.physical_device, &props);
	printf("Memory usage %f MB\n", budget_props.heapUsage[0] * 1e-6);
}

void RayTracer::update() {
	if (instance->window->is_key_down(KeyInput::KEY_F10)) {
		write_exr = true;
	}
	float frame_time = draw_frame();
	cpu_avg_time = (1.0f - 1.0f / (cnt)) * cpu_avg_time + frame_time / (float)cnt;
	cpu_avg_time = 0.95f * cpu_avg_time + 0.05f * frame_time;

	integrator->update();
}

void RayTracer::render(uint32_t i) {
	// Render image

	integrator->render();
	auto cmdbuf = vkb.ctx.command_buffers[i];
	VkCommandBufferBeginInfo begin_info = vk::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vk::check(vkBeginCommandBuffer(cmdbuf, &begin_info));
	// if (write_exr) {
	//	// Copy to host visible storage buffer
	//	{
	//		VkBufferImageCopy region = {};
	//		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//		region.imageSubresource.mipLevel = 0;
	//		region.imageSubresource.baseArrayLayer = 0;
	//		region.imageSubresource.layerCount = 1;
	//		region.imageExtent.width = instance->width;
	//		region.imageExtent.height = instance->height;
	//		region.imageExtent.depth = 1;
	//		transition_image_layout(cmdbuf, integrator->output_tex.img,
	//								VK_IMAGE_LAYOUT_GENERAL,
	// VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL); vkCmdCopyImageToBuffer(cmdbuf,
	// integrator->output_tex.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	//							   output_img_buffer_cpu.handle, 1, &region);
	//		transition_image_layout(cmdbuf, integrator->output_tex.img,
	//								VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	// VK_IMAGE_LAYOUT_GENERAL);
	//	}
	// }
	// if (calc_rmse && has_gt) {
	//	// Calculate RMSE
	//	{
	//		VkBufferImageCopy region = {};
	//		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//		region.imageSubresource.mipLevel = 0;
	//		region.imageSubresource.baseArrayLayer = 0;
	//		region.imageSubresource.layerCount = 1;
	//		region.imageExtent.width = instance->width;
	//		region.imageExtent.height = instance->height;
	//		region.imageExtent.depth = 1;
	//		transition_image_layout(cmdbuf, integrator->output_tex.img,
	//								VK_IMAGE_LAYOUT_GENERAL,
	// VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL); vkCmdCopyImageToBuffer(cmdbuf,
	// integrator->output_tex.img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	//							   output_img_buffer.handle, 1, &region);

	//		auto barrier = buffer_barrier(output_img_buffer.handle,
	//									  VK_ACCESS_TRANSFER_WRITE_BIT,
	//									  VK_ACCESS_SHADER_READ_BIT);
	//		vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
	// VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 							 VK_DEPENDENCY_BY_REGION_BIT,
	// 0, 0, 1, &barrier, 0, 0);
	//		// Calculate and reduce
	//		{

	//			vkCmdBindDescriptorSets(
	//				cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE,
	// calc_rmse_pipeline->pipeline_layout, 				0, 1, &post_desc_set, 0,
	// nullptr); 			vkCmdBindDescriptorSets( 				cmdbuf,
	// VK_PIPELINE_BIND_POINT_COMPUTE, reduce_rmse_pipeline->pipeline_layout,
	// 0, 1, &post_desc_set, 0, nullptr); 			vkCmdBindDescriptorSets( 				cmdbuf,
	// VK_PIPELINE_BIND_POINT_COMPUTE, output_rmse_pipeline->pipeline_layout,
	// 0, 1, &post_desc_set, 0, nullptr); 			vkCmdPushConstants(cmdbuf,
	// calc_rmse_pipeline->pipeline_layout, 							   VK_SHADER_STAGE_COMPUTE_BIT,
	// 0, sizeof(PostPC), &post_pc); 			vkCmdPushConstants(cmdbuf,
	// reduce_rmse_pipeline->pipeline_layout,
	// VK_SHADER_STAGE_COMPUTE_BIT, 							   0,
	// sizeof(PostPC), &post_pc); 			vkCmdPushConstants(cmdbuf,
	// output_rmse_pipeline->pipeline_layout,
	// VK_SHADER_STAGE_COMPUTE_BIT, 							   0,
	// sizeof(PostPC), &post_pc);

	//			reduce(cmdbuf, residual_buffer, counter_buffer,
	//*calc_rmse_pipeline,
	//*reduce_rmse_pipeline, 				   instance->width *
	// instance->height); 			vkCmdBindPipeline(cmdbuf,
	// VK_PIPELINE_BIND_POINT_COMPUTE, 							  output_rmse_pipeline->handle); 			auto
	// num_wgs = 1; 			vkCmdDispatch(cmdbuf, num_wgs, 1, 1);
	//			transition_image_layout(cmdbuf, integrator->output_tex.img,
	//									VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
	// VK_IMAGE_LAYOUT_GENERAL);
	//		}

	//	}
	//}

	pc_post_settings.enable_tonemapping = settings.enable_tonemapping;
	instance->vkb.rg
		->add_gfx("Post FX", {.width = instance->width,
							  .height = instance->height,
							  .clear_color = {0.25f, 0.25f, 0.25f, 1.0f},
							  .clear_depth_stencil = {1.0f, 0},
							  .shaders = {{"src/shaders/post.vert"}, {"src/shaders/post.frag"}},
							  .cull_mode = VK_CULL_MODE_NONE,
							  .color_outputs = {&vkb.swapchain_images[i]},
							  .pass_func =
								  [](VkCommandBuffer cmd, const RenderPass& render_pass) {
									  vkCmdDraw(cmd, 3, 1, 0, 0);
									  /*	ImGui::Render();
										  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
										 cmd);*/
								  }})
		.push_constants(&pc_post_settings)
		//.read(integrator->output_tex) // Needed if shader inference is off
		.bind(integrator->output_tex, integrator->texture_sampler);

	vkb.rg->run(cmdbuf);

	vk::check(vkEndCommandBuffer(cmdbuf), "Failed to record command buffer");
}

float RayTracer::draw_frame() {
	if (cnt == 0) {
		start = clock();
	}
	auto t_begin = glfwGetTime() * 1000;
	bool updated = false;
	// ImGui_ImplGlfw_NewFrame();
	// ImGui::NewFrame();
	// ImGui::Text("Frame time %f ms ( %f FPS )", cpu_avg_time,
	//			1000 / cpu_avg_time);
	// if (ImGui::Button("Reload shaders")) {
	//	integrator->reload();
	//	updated |= true;
	// }

	bool gui_updated = integrator->gui();
	// updated |=
	//	ImGui::Checkbox("Enable ACES tonemapping",
	//&settings.enable_tonemapping);
	if (updated || gui_updated) {
		// ImGui::Render();
		auto t_end = glfwGetTime() * 1000;
		auto t_diff = t_end - t_begin;
		integrator->updated = true;
		return (float)t_diff;
	}

	// ImGui::Checkbox("Show camera statistics", &show_cam_stats);

	// if (show_cam_stats) {
	//		ImGui::PushItemWidth(170);
	//		ImGui::DragFloat4("", glm::value_ptr(integrator->camera->camera[0]),
	// 0.05f); 		ImGui::DragFloat4("",
	// glm::value_ptr(integrator->camera->camera[1]), 0.05f);
	//		ImGui::DragFloat4("", glm::value_ptr(integrator->camera->camera[2]),
	// 0.05f); 		ImGui::DragFloat4("",
	// glm::value_ptr(integrator->camera->camera[3]), 0.05f);

	//}

	uint32_t image_idx = vkb.prepare_frame();

	if (image_idx == UINT32_MAX) {
		auto t_end = glfwGetTime() * 1000;
		auto t_diff = t_end - t_begin;
		return (float)t_diff;
	}
	render(image_idx);
	vkb.submit_frame(image_idx, resized);
	vkb.rg->reset(vkb.ctx.command_buffers[image_idx]);

	auto now = clock();
	auto diff = ((float)now - start);

	if (write_exr) {
		write_exr = false;
		save_exr((float*)output_img_buffer_cpu.data, instance->width, instance->height, "out.exr");
	}
	bool time_limit = (abs(diff / CLOCKS_PER_SEC - 5)) < 0.1;
	// calc_rmse = cnt % 30 == 0 || time_limit;
	// calc_rmse = time_limit;
	// bool t2 = (abs(diff / CLOCKS_PER_SEC - 10.0)) < 0.1;
	// if (t2) {
	//	printf("Go!\n");
	//	t2 = false;
	// }
	// printf("Time %f\n", diff / CLOCKS_PER_SEC);
	// calc_rmse = true;
	// write_exr = time_limit;
	if (calc_rmse && has_gt) {
		float rmse = *(float*)rmse_val_buffer.data;
		printf("RMSE %f - Diff %f\n", rmse * 1e6, diff);
	}
	auto t_end = glfwGetTime() * 1000;
	auto t_diff = t_end - t_begin;
	cnt++;
	return (float)t_diff;
}

void RayTracer::create_post_descriptor() {
	// constexpr int OUTPUT_COLOR_BINDING = 0;
	// constexpr int POST_DESC_BINDING = 1;
	// std::vector<VkDescriptorPoolSize> pool_sizes = {
	//	vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
	//	vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1), };
	// auto descriptor_pool_ci =
	//	vk::descriptor_pool_CI(pool_sizes.size(), pool_sizes.data(), 1);
	// vk::check(vkCreateDescriptorPool(vkb.ctx.device, &descriptor_pool_ci,
	//		  nullptr, &post_desc_pool),
	//		  "Failed to create descriptor pool");

	// std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
	//	vk::descriptor_set_layout_binding(
	//		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	//		VK_SHADER_STAGE_FRAGMENT_BIT, OUTPUT_COLOR_BINDING),
	//	vk::descriptor_set_layout_binding(
	//		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	//		VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
	// POST_DESC_BINDING),
	// };
	// auto set_layout_ci = vk::descriptor_set_layout_CI(
	//	set_layout_bindings.data(), set_layout_bindings.size());
	// vk::check(vkCreateDescriptorSetLayout(vkb.ctx.device, &set_layout_ci,
	//		  nullptr, &post_desc_layout),
	//		  "Failed to create descriptor set layout");

	// auto set_allocate_info =
	//	vk::descriptor_set_allocate_info(post_desc_pool, &post_desc_layout, 1);
	// vk::check(vkAllocateDescriptorSets(vkb.ctx.device, &set_allocate_info,
	//		  &post_desc_set),
	//		  "Failed to allocate descriptor sets");
}

void RayTracer::update_post_desc_set() {
	// std::array<VkWriteDescriptorSet, 2> sets = {
	//	vk::write_descriptor_set(
	//		post_desc_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0,
	//		&integrator->output_tex.descriptor_image_info),
	//	vk::write_descriptor_set(
	//		post_desc_set, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
	//		&post_desc_buffer.descriptor)
	// };
	// vkUpdateDescriptorSets(vkb.ctx.device, sets.size(), sets.data(), 0,
	// nullptr);
}

void RayTracer::create_post_pipeline() {
	// GraphicsPipelineSettings post_settings;
	// VkPipelineLayoutCreateInfo create_info{
	//	VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

	// create_info.setLayoutCount = 1;
	// create_info.pSetLayouts = &post_desc_layout;
	// create_info.pushConstantRangeCount = 1;
	// VkPushConstantRange pc_range = {
	//	VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantPost)
	// };
	// create_info.pPushConstantRanges = &pc_range;
	// vkCreatePipelineLayout(vkb.ctx.device, &create_info, nullptr,
	//					   &post_pipeline_layout);

	// post_settings.pipeline_layout = post_pipeline_layout;
	// post_settings.render_pass = vkb.ctx.default_render_pass;
	// post_settings.shaders = { {"src/shaders/post.vert"},
	//						 {"src/shaders/post.frag"} };
	// for (auto& shader : post_settings.shaders) {
	//	if (shader.compile()) {
	//		LUMEN_ERROR("Shader compilation failed");
	//	}
	// }
	// post_settings.cull_mode = VK_CULL_MODE_NONE;
	// post_settings.enable_tracking = false;
	// post_settings.dynamic_state_enables = { VK_DYNAMIC_STATE_VIEWPORT,
	//									   VK_DYNAMIC_STATE_SCISSOR };
	// post_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	// post_pipeline->create_gfx_pipeline(post_settings);
}

void RayTracer::create_compute_pipelines() {
	// calc_rmse_pipeline =
	// std::make_unique<Pipeline>(instance->vkb.ctx.device);
	// reduce_rmse_pipeline =
	// std::make_unique<Pipeline>(instance->vkb.ctx.device);
	// output_rmse_pipeline =
	// std::make_unique<Pipeline>(instance->vkb.ctx.device); std::vector<Shader>
	// shaders = {
	//	{"src/shaders/rmse/calc_rmse.comp"},
	//	{"src/shaders/rmse/reduce_rmse.comp"},
	//	{"src/shaders/rmse/output_rmse.comp"}
	// };
	// for (auto& shader : shaders) {
	//	shader.compile();
	// }
	// ComputePipelineSettings settings;
	// settings.desc_set_layouts = &post_desc_layout;
	// settings.desc_set_layout_cnt = 1;
	// settings.push_const_size = sizeof(PostPC);
	// settings.shader = shaders[0];
	// calc_rmse_pipeline->create_compute_pipeline(settings);
	// settings.shader = shaders[1];
	// reduce_rmse_pipeline->create_compute_pipeline(settings);
	// settings.shader = shaders[2];
	// output_rmse_pipeline->create_compute_pipeline(settings);
}

void RayTracer::init_imgui() {
	VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
										 {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
										 {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
										 {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	vk::check(vkCreateDescriptorPool(vkb.ctx.device, &pool_info, nullptr, &imgui_pool));
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	// Setup Platform/Renderer backends
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForVulkan(window->get_window_ptr(), true);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = vkb.ctx.instance;
	init_info.PhysicalDevice = vkb.ctx.physical_device;
	init_info.Device = vkb.ctx.device;
	init_info.Queue = vkb.ctx.queues[(int)QueueType::GFX];
	init_info.DescriptorPool = imgui_pool;
	init_info.MinImageCount = 2;
	init_info.ImageCount = 2;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info, vkb.ctx.default_render_pass);

	CommandBuffer cmd(&vkb.ctx, true);
	ImGui_ImplVulkan_CreateFontsTexture(cmd.handle);
	cmd.submit(vkb.ctx.queues[(int)QueueType::GFX]);
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void RayTracer::init_resources() {
	PostDesc desc;
	output_img_buffer.create(&instance->vkb.ctx,
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
							 instance->width * instance->height * 4 * 4);

	output_img_buffer_cpu.create(&instance->vkb.ctx,
								 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								 VK_SHARING_MODE_EXCLUSIVE, instance->width * instance->height * 4 * 4);
	residual_buffer.create(&instance->vkb.ctx,
						   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
						   instance->width * instance->height * 4);

	counter_buffer.create(&instance->vkb.ctx,
						  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, sizeof(int));

	rmse_val_buffer.create(&instance->vkb.ctx,
						   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						   VK_SHARING_MODE_EXCLUSIVE, sizeof(float));
	if (load_exr) {
		// Load the ground truth image
		const char* img_name = "out.exr";
		float* data;
		int width;
		int height;
		const char* err = nullptr;

		int ret = LoadEXR(&data, &width, &height, img_name, &err);
		if (ret != TINYEXR_SUCCESS) {
			if (err) {
				fprintf(stderr, "ERR : %s\n", err);
				FreeEXRErrorMessage(err);  // release memory of error message.
			}
		} else {
			std::vector<vec4> pixels;
			int img_res = width * height;
			pixels.resize(img_res);
			for (int i = 0; i < img_res; i++) {
				pixels[i].x = data[4 * i + 0];
				pixels[i].y = data[4 * i + 1];
				pixels[i].z = data[4 * i + 2];
				pixels[i].w = 1.;
			}
			auto gt_size = pixels.size() * 4 * 4;
			gt_img_buffer.create(
				&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, gt_size, pixels.data(), true);
			desc.gt_img_addr = gt_img_buffer.get_device_address();
			has_gt = true;
		}
		if (has_gt) {
			free(data);
		}
	}
	desc.out_img_addr = output_img_buffer.get_device_address();
	desc.residual_addr = residual_buffer.get_device_address();
	desc.counter_addr = counter_buffer.get_device_address();
	desc.rmse_val_addr = rmse_val_buffer.get_device_address();
	post_desc_buffer.create(
		&instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, sizeof(PostDesc), &desc, true);

	post_pc.size = instance->width * instance->height;
}

void RayTracer::parse_args(int argc, char* argv[]) {
	scene_name = "scenes/caustics.json";
	std::regex fn("(.*).(.json|.xml)");
	for (int i = 0; i < argc; i++) {
		if (std::regex_match(argv[i], fn)) {
			scene_name = argv[i];
		}
	}
}

void RayTracer::save_exr(const float* rgb, int width, int height, const char* outfilename) {
	EXRHeader header;
	InitEXRHeader(&header);

	EXRImage image;
	InitEXRImage(&image);

	image.num_channels = 3;

	std::vector<float> images[3];
	images[0].resize(width * height);
	images[1].resize(width * height);
	images[2].resize(width * height);

	// Split RGBRGBRGB... into R, G and B layer
	for (int i = 0; i < width * height; i++) {
		images[0][i] = rgb[4 * i + 0];
		images[1][i] = rgb[4 * i + 1];
		images[2][i] = rgb[4 * i + 2];
	}

	float* image_ptr[3];
	image_ptr[0] = &(images[2].at(0));	// B
	image_ptr[1] = &(images[1].at(0));	// G
	image_ptr[2] = &(images[0].at(0));	// R

	image.images = (unsigned char**)image_ptr;
	image.width = width;
	image.height = height;

	header.num_channels = 3;
	header.channels = (EXRChannelInfo*)malloc(sizeof(EXRChannelInfo) * header.num_channels);
	// Must be (A)BGR order, since most of EXR viewers expect this channel
	// order.
	strncpy_s(header.channels[0].name, "B", 255);
	header.channels[0].name[strlen("B")] = '\0';
	strncpy_s(header.channels[1].name, "G", 255);
	header.channels[1].name[strlen("G")] = '\0';
	strncpy_s(header.channels[2].name, "R", 255);
	header.channels[2].name[strlen("R")] = '\0';

	header.pixel_types = (int*)malloc(sizeof(int) * header.num_channels);
	header.requested_pixel_types = (int*)malloc(sizeof(int) * header.num_channels);
	for (int i = 0; i < header.num_channels; i++) {
		header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;		   // pixel type of input image
		header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_HALF;  // pixel type of output image to be stored
																   // in .EXR
	}

	const char* err = NULL;	 // or nullptr in C++11 or later.
	int ret = SaveEXRImageToFile(&image, &header, outfilename, &err);
	if (ret != TINYEXR_SUCCESS) {
		fprintf(stderr, "Save EXR err: %s\n", err);
		FreeEXRErrorMessage(err);  // free's buffer for an error message
	}
	printf("Saved exr file. [ %s ] \n", outfilename);

	free(header.channels);
	free(header.pixel_types);
	free(header.requested_pixel_types);
}

void RayTracer::cleanup() {
	const auto device = vkb.ctx.device;
	vkDeviceWaitIdle(device);
	if (initialized) {
		vkDestroyDescriptorSetLayout(device, post_desc_layout, nullptr);
		vkDestroyDescriptorPool(device, post_desc_pool, nullptr);
		vkDestroyDescriptorPool(device, imgui_pool, nullptr);
		vkDestroyPipelineLayout(device, post_pipeline_layout, nullptr);
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		gt_img_buffer.destroy();
		output_img_buffer.destroy();
		integrator->destroy();
		post_pipeline->cleanup();
		vkb.cleanup();
	}
}
