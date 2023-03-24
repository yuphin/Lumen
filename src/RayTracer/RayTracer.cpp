#include "LumenPCH.h"
#include <regex>
#include <stb_image.h>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include "RayTracer.h"
#include <complex>
#include <tinyexr.h>

RayTracer* RayTracer::instance = nullptr;
bool load_reference = false;
bool calc_rmse = false;

#define FFT_DEBUG 1

static glm::vec2 complex_mul(const glm::vec2& a, const glm::vec2& b) {
	return glm::vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

static void fft(int n, glm::vec2* x, glm::vec2* y) {
	const int m = n / 2;
	for (int stride = 1; stride < n; stride *= 2) {
		for (int j = 0; j < m; j++) {
			auto stride_factor = stride * (j / stride);
			const auto angle = 2 * glm::pi<float>() * stride_factor / n;
			auto wp = glm::vec2(std::cos(angle), -std::sin(angle));
			auto& a = x[j];
			auto& b = x[j + m];
			auto fixed_idx = j % stride + 2 * stride_factor;
			y[fixed_idx] = a + b;
			y[fixed_idx + stride] = complex_mul(a - b, wp);
		}
		std::swap(x, y);
	}
}

static void do_fft(std::vector<glm::vec2>& x, std::vector<glm::vec2>& y) {
	fft(x.size(), x.data(), y.data());
	uint32_t num_passes = floor(log2(x.size()));
	if (num_passes % 2) {
		std::swap(x, y);
	}
}

static void fb_resize_callback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<RayTracer*>(glfwGetWindowUserPointer(window));
	app->resized = true;
}

RayTracer::RayTracer(int width, int height, bool debug, int argc, char* argv[]) : LumenInstance(width, height, debug) {
	this->instance = this;
	parse_args(argc, argv);
}

void RayTracer::init(Window* window) {
	// srand((uint32_t)time(NULL));
	srand(42);
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
	vkb.init_imgui();
	initialized = true;

	scene.load_scene(scene_name);

	// Enable shader reflections for the render graph
	vkb.rg->settings.shader_inference = true;
	// Disable event based synchronization
	// Currently the event API that comes with Vulkan 1.3 is buggy on NVIDIA drivers
	// so this is turned off and pipeline barriers are used instead
	vkb.rg->settings.use_events = true;

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
	printf("Memory usage %f MB\n", get_memory_usage(vk_ctx.physical_device) * 1e-6);
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

	fft_arr.resize(FFT_SIZE);
	std::vector<glm::vec2> y(fft_arr.size());
	for (int i = 0; i < fft_arr.size(); i++) {
		const float r1 = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		const float r2 = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		fft_arr[i] = {r1, r2};
	}
	fft_buffers[0].create("FFT Ping", &instance->vkb.ctx,
						  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
						  2 * fft_arr.size() * sizeof(float), fft_arr.data(), true);

	fft_buffers[1].create("FFT Pong", &instance->vkb.ctx,
						  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
							  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE,
						  2 * fft_arr.size() * sizeof(float));

	fft_cpu_buffers[0].create("FFT Ping CPU", &instance->vkb.ctx,
							  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
							  VK_SHARING_MODE_EXCLUSIVE, 2 * fft_arr.size() * sizeof(float));

	fft_cpu_buffers[1].create("FFT Pong CPU", &instance->vkb.ctx,
							  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
							  VK_SHARING_MODE_EXCLUSIVE, 2 * fft_arr.size() * sizeof(float));

	do_fft(fft_arr, y);
	if (load_reference) {
		// Load the ground truth image
		int width, height;
		float* data = load_exr("out.exr", width, height);
		if (!data) {
			LUMEN_ERROR("Could not load the reference image");
		}
		auto gt_size = width * height * 4 * sizeof(float);
		gt_img_buffer.create("Ground Truth Image", &instance->vkb.ctx,
							 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, gt_size, data, true);
		desc.gt_img_addr = gt_img_buffer.get_device_address();
		free(data);
	}
	// Lena
	{
		VkSamplerCreateInfo sampler_ci = vk::sampler_create_info();
		sampler_ci.minFilter = VK_FILTER_NEAREST;
		sampler_ci.magFilter = VK_FILTER_NEAREST;
		sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler_ci.maxLod = FLT_MAX;

		sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		vk::check(vkCreateSampler(instance->vkb.ctx.device, &sampler_ci, nullptr, &lena_sampler),
				  "Could not create image sampler");
		const char* img_name_kernel = "gaussian.exr";
		const char* img_name = "test512.png";
		int x, y, n;
		unsigned char* img_data = stbi_load(img_name, &x, &y, &n, 4);

		auto size = x * y * 4;
		auto img_dims = VkExtent2D{(uint32_t)x, (uint32_t)y};
		auto ci = make_img2d_ci(img_dims, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT, false);
		lena_ping.load_from_data(&instance->vkb.ctx, img_data, size, ci, lena_sampler,
								 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, false);
		stbi_image_free(img_data);

		ci.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		int width, height;
		float* data = load_exr(img_name_kernel, width, height);
		kernel_ping.load_from_data(&instance->vkb.ctx, data, width * height * 4 * sizeof(float), ci, lena_sampler,
								   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, false);
		if (data) {
			free(data);
		}
		TextureSettings settings;
		settings.base_extent = lena_ping.base_extent;
		settings.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		settings.usage_flags = lena_ping.usage_flags;
		lena_pong.create_empty_texture("Lena - Pong", &instance->vkb.ctx, settings, VK_IMAGE_LAYOUT_GENERAL,
									   lena_sampler);
		kernel_pong.create_empty_texture("Kernel - Pong", &instance->vkb.ctx, settings, VK_IMAGE_LAYOUT_GENERAL,
										 lena_sampler);
	}

	desc.out_img_addr = output_img_buffer.get_device_address();
	desc.residual_addr = residual_buffer.get_device_address();
	desc.counter_addr = counter_buffer.get_device_address();
	desc.rmse_val_addr = rmse_val_buffer.get_device_address();
	post_desc_buffer.create(
		"Post Desc", &instance->vkb.ctx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_SHARING_MODE_EXCLUSIVE, sizeof(PostDesc), &desc, true);
	post_pc.size = instance->width * instance->height;
	REGISTER_BUFFER_WITH_ADDRESS(PostDesc, desc, out_img_addr, &output_img_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(PostDesc, desc, residual_addr, &residual_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(PostDesc, desc, counter_addr, &counter_buffer, instance->vkb.rg);
	REGISTER_BUFFER_WITH_ADDRESS(PostDesc, desc, rmse_val_addr, &rmse_val_buffer, instance->vkb.rg);

	const bool FFT_SHARED_MEM = true;
	const uint32_t WG_SIZE_X = kernel_ping.base_extent.width;
	auto dim_x = (uint32_t)(kernel_ping.base_extent.width * kernel_ping.base_extent.height + WG_SIZE_X - 1) / WG_SIZE_X;
	bool vertical = false;

	CommandBuffer cmd(&vkb.ctx, true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	instance->vkb.rg
		->add_compute("FFT - Horizontal - Kernel",
					  {.shader = Shader("src/shaders/fft/fft.comp"),
					   .specialization_data = {WG_SIZE_X >> 1, uint32_t(FFT_SHARED_MEM), uint32_t(vertical), 0, 1, 1},
					   .dims = {dim_x, 1, 1}})
		.bind({post_desc_buffer, fft_buffers[0], fft_buffers[1]})
		.bind(kernel_ping, lena_sampler)
		.bind(kernel_pong)
		.bind(kernel_ping, lena_sampler)
		.push_constants(&fft_pc);
	vertical = true;
	instance->vkb.rg
		->add_compute("FFT - Vertical - Kernel",
					  {.shader = Shader("src/shaders/fft/fft.comp"),
					   .specialization_data = {WG_SIZE_X >> 1, uint32_t(FFT_SHARED_MEM), uint32_t(vertical), 0, 1, 1},
					   .dims = {dim_x, 1, 1}})
		.bind({post_desc_buffer, fft_buffers[0], fft_buffers[1]})
		.bind(kernel_ping, lena_sampler)
		.bind(kernel_pong)
		.bind(kernel_ping, lena_sampler)
		.push_constants(&fft_pc);
	vkb.rg->run_and_submit(cmd);
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
#if FFT_DEBUG
	if (cnt == 0) {
		// if (1) {
		int num_iters = log2(fft_arr.size());

		const bool FFT_SHARED_MEM = true;

		fft_pc.n = fft_arr.size();
		if (FFT_SHARED_MEM) {
			const uint32_t WG_SIZE_X = lena_ping.base_extent.width;
			auto dim_x =
				(uint32_t)(lena_ping.base_extent.width * lena_ping.base_extent.height + WG_SIZE_X - 1) / WG_SIZE_X;
			bool vertical = false;
			instance->vkb.rg
				->add_compute("FFT - Horizontal", {.shader = Shader("src/shaders/fft/fft.comp"),
												   .specialization_data = {WG_SIZE_X >> 1, uint32_t(FFT_SHARED_MEM),
																		   uint32_t(vertical), 0, 1},
												   .dims = {dim_x, 1, 1}})
				.bind({post_desc_buffer, fft_buffers[0], fft_buffers[1]})
				.bind(lena_ping, lena_sampler)
				.bind(lena_pong)
				.bind(kernel_pong, lena_sampler)
				.push_constants(&fft_pc);
			vertical = true;
			instance->vkb.rg
				->add_compute("FFT - Vertical", {.shader = Shader("src/shaders/fft/fft.comp"),
												 .specialization_data = {WG_SIZE_X >> 1, uint32_t(FFT_SHARED_MEM),
																		 uint32_t(vertical), 0, 0},
												 .dims = {dim_x, 1, 1}})
				.bind({post_desc_buffer, fft_buffers[0], fft_buffers[1]})
				.bind(lena_ping, lena_sampler)
				.bind(lena_pong)
				.bind(kernel_pong, lena_sampler)
				.push_constants(&fft_pc);
			instance->vkb.rg
				->add_compute(
					"FFT - Vertical - Inverse",
					{.shader = Shader("src/shaders/fft/fft.comp"),
					 .specialization_data = {WG_SIZE_X >> 1, uint32_t(FFT_SHARED_MEM), uint32_t(vertical), 1, 1},
					 .dims = {dim_x, 1, 1}})
				.bind({post_desc_buffer, fft_buffers[0], fft_buffers[1]})
				.bind(lena_ping, lena_sampler)
				.bind(lena_pong)
				.bind(kernel_pong, lena_sampler)
				.push_constants(&fft_pc);
			vertical = false;
			instance->vkb.rg
				->add_compute(
					"FFT - Horizontal - Inverse",
					{.shader = Shader("src/shaders/fft/fft.comp"),
					 .specialization_data = {WG_SIZE_X >> 1, uint32_t(FFT_SHARED_MEM), uint32_t(vertical), 1, 1},
					 .dims = {dim_x, 1, 1}})
				.bind({post_desc_buffer, fft_buffers[0], fft_buffers[1]})
				.bind(lena_ping, lena_sampler)
				.bind(lena_pong)
				.bind(kernel_pong, lena_sampler)
				.push_constants(&fft_pc);
		} else {
			const uint32_t WG_SIZE_X = 4;
			auto dim_x = (uint32_t)(fft_arr.size() / 2 + WG_SIZE_X - 1) / WG_SIZE_X;
			for (int i = 0; i < num_iters; i++) {
				bool pingpong = i % 2;
				fft_pc.idx = i;
				fft_pc.n = fft_arr.size();
				instance->vkb.rg
					->add_compute("FFT", {.shader = Shader("src/shaders/fft/fft.comp"),
										  .specialization_data = {WG_SIZE_X, uint32_t(FFT_SHARED_MEM)},
										  .dims = {dim_x, 1, 1}})
					.bind({post_desc_buffer, fft_buffers[pingpong], fft_buffers[!pingpong]})
					.push_constants(&fft_pc);
			}
		}

#if 0
		instance->vkb.rg->current_pass()
			.copy(fft_buffers[0], fft_cpu_buffers[0])
			.copy(fft_buffers[1], fft_cpu_buffers[1]);
		instance->vkb.rg->run_and_submit(cmd);

		std::vector<glm::vec2> fft_ping, fft_pong;
		fft_ping.assign((glm::vec2*)fft_cpu_buffers[0].data,
						(glm::vec2*)fft_cpu_buffers[0].data + fft_cpu_buffers[0].size / sizeof(glm::vec2));
		fft_pong.assign((glm::vec2*)fft_cpu_buffers[1].data,
						(glm::vec2*)fft_cpu_buffers[1].data + fft_cpu_buffers[1].size / sizeof(glm::vec2));

		std::vector<glm::vec2>& res = (FFT_SHARED_MEM || num_iters % 2) ? fft_pong : fft_ping;
		for (int i = 0; i < res.size(); i++) {
			auto diff = res[i] - fft_arr[i];
			if (glm::dot(diff, diff) > 1e-3) {
				LUMEN_ERROR("Error");
			}
		}
#endif
	}
#endif
#if !FFT_DEBUG
	// Render image
	integrator->render();
#endif
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
							  .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
							  .color_outputs = {&vkb.swapchain_images[i]},
							  .pass_func =
								  [](VkCommandBuffer cmd, const RenderPass& render_pass) {
									  vkCmdDraw(cmd, 4, 1, 0, 0);
									  ImGui::Render();
									  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
								  }})
		.push_constants(&pc_post_settings)
		//.read(integrator->output_tex) // Needed if shader inference is off
		.bind(integrator->output_tex, integrator->texture_sampler)
		.bind(lena_pong, lena_sampler)
		.bind(kernel_pong, lena_sampler);

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
				num_wgs = (num_wgs + 1023) / 1024;
			}
		};
		instance->vkb.rg->current_pass().copy(integrator->output_tex, output_img_buffer);
		// Calculate RMSE
		op_reduce("OpReduce: RMSE", "src/shaders/rmse/calc_rmse.comp", "OpReduce: Reduce RMSE",
				  "src/shaders/rmse/reduce_rmse.comp");
		instance->vkb.rg
			->add_compute("Calculate RMSE", {.shader = Shader("src/shaders/rmse/output_rmse.comp"), .dims = {1, 1, 1}})
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
	ImGui::Text("Memory Usage: %f MB", get_memory_usage(vk_ctx.physical_device) * 1e-6);

	bool gui_updated = integrator->gui();
	updated |= ImGui::Checkbox("Enable ACES tonemapping", &settings.enable_tonemapping);

	ImGui::Checkbox("Show camera statistics", &show_cam_stats);
	if (show_cam_stats) {
		ImGui::PushItemWidth(170);
		ImGui::DragFloat4("", glm::value_ptr(integrator->camera->camera[0]), 0.05f);
		ImGui::DragFloat4("", glm::value_ptr(integrator->camera->camera[1]), 0.05f);
		ImGui::DragFloat4("", glm::value_ptr(integrator->camera->camera[2]), 0.05f);
		ImGui::DragFloat4("", glm::value_ptr(integrator->camera->camera[3]), 0.05f);
	}
	if (ImGui::Button("Reload shaders")) {
		// TODO
		vkb.rg->reload_shaders = true;
		vkb.rg->shader_cache.clear();
		updated |= true;
	}

	if (updated || gui_updated) {
		ImGui::Render();
		auto t_end = glfwGetTime() * 1000;
		auto t_diff = t_end - t_begin;
		integrator->updated = true;
		return (float)t_diff;
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

void RayTracer::parse_args(int argc, char* argv[]) {
	scene_name = "scenes/caustics.json";
	std::regex fn("(.*).(.json|.xml)");
	for (int i = 0; i < argc; i++) {
		if (std::regex_match(argv[i], fn)) {
			scene_name = argv[i];
		}
	}
}
void RayTracer::cleanup() {
	const auto device = vkb.ctx.device;
	vkDeviceWaitIdle(device);
	if (initialized) {
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		std::vector<Buffer*> buffer_list = {&output_img_buffer, &output_img_buffer_cpu, &residual_buffer,
											&counter_buffer,	&rmse_val_buffer,		&post_desc_buffer};
		if (load_reference) {
			buffer_list.push_back(&gt_img_buffer);
		}
		for (auto b : buffer_list) {
			b->destroy();
		}
		integrator->destroy();
		vkb.cleanup();
	}
}
