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
bool load_exr = false;
bool calc_rmse = false;
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
	vkb.create_command_pools();
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
		case IntegratorType::VCM:
			integrator = std::make_unique<VCM>(this, &scene);
			break;
		case IntegratorType::ReSTIR:
			integrator = std::make_unique<ReSTIR>(this, &scene);
			break;
		case IntegratorType::ReSTIRGI:
			integrator = std::make_unique<ReSTIRGI>(this, &scene);
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
		case IntegratorType::DDGI:
			integrator = std::make_unique<DDGI>(this, &scene);
			break;
		default:
			break;
	}
	integrator->init();
	init_resources();
	init_imgui();
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
									  ImGui::Render();
									  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
								  }})
		.push_constants(&pc_post_settings)
		//.read(integrator->output_tex) // Needed if shader inference is off
		.bind(integrator->output_tex, integrator->texture_sampler);

	if (write_exr) {
		instance->vkb.rg->current_pass().copy(integrator->output_tex, output_img_buffer_cpu);
	}
	if (calc_rmse && has_gt) {
		auto op_reduce = [&](const std::string& op_name, const std::string& op_shader_name,
							 const std::string& reduce_name, const std::string& reduce_shader_name) {
			uint32_t num_wgs = uint32_t((instance->width * instance->height + 1023) / 1024);
			instance->vkb.rg->add_compute(op_name, {.shader = Shader(op_shader_name), .dims = {num_wgs, 1, 1}})
				.push_constants(&post_pc)
				.bind(post_desc_buffer)
				.zero({residual_buffer, counter_buffer});
			while (num_wgs != 1) {
				instance->vkb.rg
					->add_compute(reduce_name, {.shader = Shader(reduce_shader_name), .dims = {num_wgs, 1, 1}})
					.push_constants(&post_pc)
					.bind(post_desc_buffer);
				num_wgs = (uint32_t)(num_wgs + 1023) / 1024.0f;
			}
		};
		instance->vkb.rg->current_pass().copy(integrator->output_tex, output_img_buffer);
		// Calculate RMSE
		op_reduce("OpReduce: RMSE", "src/shaders/rmse/calc_rmse.comp", "OpReduce: Reduce RMSE",
				  "src/shaders/rmse/reduce_rmse.comp");
		instance->vkb.rg
			->add_compute("Calculate RMSE", { .shader = Shader("src/shaders/rmse/output_rmse.comp"), .dims = {1, 1, 1} })
			.push_constants(&post_pc)
			.bind(post_desc_buffer);
	}

	vkb.rg->run(cmdbuf);

	vk::check(vkEndCommandBuffer(cmdbuf), "Failed to record command buffer");
}

float RayTracer::draw_frame() {
	if (cnt == 0) {
		start = clock();
	}
	auto t_begin = glfwGetTime() * 1000;
	bool updated = false;
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Text("Frame time %f ms ( %f FPS )", cpu_avg_time, 1000 / cpu_avg_time);
	if (ImGui::Button("Reload shaders")) {
		// TODO
		updated |= true;
	}

	bool gui_updated = integrator->gui();
	updated |= ImGui::Checkbox("Enable ACES tonemapping", &settings.enable_tonemapping);
	if (updated || gui_updated) {
		ImGui::Render();
		auto t_end = glfwGetTime() * 1000;
		auto t_diff = t_end - t_begin;
		integrator->updated = true;
		return (float)t_diff;
	}

	ImGui::Checkbox("Show camera statistics", &show_cam_stats);
	if (show_cam_stats) {
		ImGui::PushItemWidth(170);
		ImGui::DragFloat4("", glm::value_ptr(integrator->camera->camera[0]), 0.05f);
		ImGui::DragFloat4("", glm::value_ptr(integrator->camera->camera[1]), 0.05f);
		ImGui::DragFloat4("", glm::value_ptr(integrator->camera->camera[2]), 0.05f);
		ImGui::DragFloat4("", glm::value_ptr(integrator->camera->camera[3]), 0.05f);
	}

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
	calc_rmse = time_limit;

	if (calc_rmse && has_gt) {
		float rmse = *(float*)rmse_val_buffer.data;
		LUMEN_TRACE("RMSE {}", rmse * 1e6);
		start = now;
	}
	auto t_end = glfwGetTime() * 1000;
	auto t_diff = t_end - t_begin;
	cnt++;
	return (float)t_diff;
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
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui_ImplGlfw_InitForVulkan(window->get_window_ptr(), true);

	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = vkb.ctx.instance;
	init_info.PhysicalDevice = vkb.ctx.physical_device;
	init_info.Device = vkb.ctx.device;
	init_info.Queue = vkb.ctx.queues[(int)QueueType::GFX];
	init_info.DescriptorPool = imgui_pool;
	init_info.MinImageCount = 3;
	init_info.ImageCount = 3;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.UseDynamicRendering = true;
	init_info.ColorAttachmentFormat = vkb.swapchain_format;

	ImGui_ImplVulkan_Init(&init_info, nullptr);

	CommandBuffer cmd(&vkb.ctx, true);
	ImGui_ImplVulkan_CreateFontsTexture(cmd.handle);
	cmd.submit(vkb.ctx.queues[(int)QueueType::GFX]);
	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void RayTracer::init_resources() {
	PostDesc desc;
	output_img_buffer.create("Output Image Buffer", &instance->vkb.ctx,
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
								 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
							 instance->width * instance->height * 4 * 4);

	output_img_buffer_cpu.create("Output Image CPU", &instance->vkb.ctx,
								 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								 VK_SHARING_MODE_EXCLUSIVE, instance->width * instance->height * 4 * 4);
	residual_buffer.create("RMSE Residual", &instance->vkb.ctx,
						   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
						   instance->width * instance->height * 4);

	counter_buffer.create("RMSE Counter", &instance->vkb.ctx,
						  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							  VK_BUFFER_USAGE_TRANSFER_DST_BIT,
						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, sizeof(int));

	rmse_val_buffer.create("RMSE Value", &instance->vkb.ctx,
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
			gt_img_buffer.create("Ground Truth Image", &instance->vkb.ctx,
								 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
								 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, gt_size, pixels.data(),
								 true);
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
	post_desc_buffer.create("Post Desc", &instance->vkb.ctx,
							VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, sizeof(PostDesc), &desc,
							true);
	post_pc.size = instance->width * instance->height;
	REGISTER_BUFFER_WITH_ADDRESS(PostDesc, desc, out_img_addr, &output_img_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(PostDesc, desc, residual_addr, &residual_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(PostDesc, desc, counter_addr, &counter_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(PostDesc, desc, rmse_val_addr, &rmse_val_buffer, instance->vkb.rg);
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
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		gt_img_buffer.destroy();
		output_img_buffer.destroy();
		integrator->destroy();
		vkb.cleanup();
	}
}
