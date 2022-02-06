#include "LumenPCH.h"
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

RayTracer::RayTracer(int width, int height, bool debug)
	: LumenInstance(width, height, debug) {
	this->instance = this;
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

	vkb.create_instance();
	if (vkb.enable_validation_layers) {
		vkb.setup_debug_messenger();
	}
	vkb.create_surface();
	vkb.pick_physical_device();
	vkb.create_logical_device();
	vkb.create_swapchain();
	create_default_render_pass(vkb.ctx);
	vkb.create_framebuffers(vkb.ctx.default_render_pass);
	vkb.create_command_pool();
	vkb.create_command_buffers();
	vkb.create_sync_primitives();
	initialized = true;
	// TODO: Parse this via the scene file
	SceneConfig config;
	
	//config.filename = "cornell_box.json";
	config.filename = "occluded.json";
	integrator = std::make_unique<VCM>(this, config);
	integrator->init();
	create_post_descriptor();
	update_post_desc_set();
	create_post_pipeline();
	init_imgui();
}

void RayTracer::update() {
	float frame_time = draw_frame();
	cpu_avg_time = 0.95f * cpu_avg_time + 0.05f * frame_time;
	integrator->update();
}

void RayTracer::render(uint32_t i) {
	// Render image
	integrator->render();
	// Apply Post FX and present
	auto cmdbuf = vkb.ctx.command_buffers[i];
	VkCommandBufferBeginInfo begin_info = vk::command_buffer_begin_info(
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vk::check(vkBeginCommandBuffer(cmdbuf, &begin_info));
	VkClearValue clear_color = { 0.25f, 0.25f, 0.25f, 1.0f };
	VkClearValue clear_depth = { 1.0f, 0 };
	VkViewport viewport = vk::viewport((float)width, (float)height, 0.0f, 1.0f);
	VkClearValue clear_values[] = { clear_color, clear_depth };

	VkRenderPassBeginInfo post_rpi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	post_rpi.clearValueCount = 2;
	post_rpi.pClearValues = clear_values;
	post_rpi.renderPass = vkb.ctx.default_render_pass;
	post_rpi.framebuffer = vkb.ctx.swapchain_framebuffers[i];
	post_rpi.renderArea = { {0, 0}, vkb.ctx.swapchain_extent };

	pc_post_settings.enable_tonemapping = settings.enable_tonemapping;
	vkCmdBeginRenderPass(cmdbuf, &post_rpi, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
	VkRect2D scissor = vk::rect2D(width, height, 0, 0);
	vkCmdSetScissor(cmdbuf, 0, 1, &scissor);
	vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
					  post_pipeline->handle);
	vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS,
							post_pipeline_layout, 0, 1, &post_desc_set, 0,
							nullptr);
	vkCmdPushConstants(cmdbuf, post_pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
					   0, sizeof(PushConstantPost), &pc_post_settings);
	vkCmdDraw(cmdbuf, 3, 1, 0, 0);
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdbuf);
	vkCmdEndRenderPass(cmdbuf);
	VkClearColorValue val = { 0,0,0,1 };
	
	VkImageSubresourceRange range;
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseMipLevel = 0;
	range.levelCount = 1;
	range.baseArrayLayer = 0;
	range.layerCount = 1;
	vk::check(vkEndCommandBuffer(cmdbuf), "Failed to record command buffer");
}

float RayTracer::draw_frame() {
	auto t_begin = glfwGetTime() * 1000;
	bool updated = false;
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Text("Frame time %f ms ( %f FPS )", cpu_avg_time,
				1000 / cpu_avg_time);
	if (ImGui::Button("Reload shaders")) {
		integrator->reload();
		updated |= true;
	}
	bool gui_updated = integrator->gui();
	updated |=
		ImGui::Checkbox("Enable ACES tonemapping", &settings.enable_tonemapping);
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
	auto t_end = glfwGetTime() * 1000;
	auto t_diff = t_end - t_begin;
	return (float)t_diff;
}

void RayTracer::create_post_descriptor() {
	constexpr int SAMPLER_COLOR_BINDING = 0;
	std::vector<VkDescriptorPoolSize> pool_sizes = {
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
	auto descriptor_pool_ci =
		vk::descriptor_pool_CI(pool_sizes.size(), pool_sizes.data(), 1);
	vk::check(vkCreateDescriptorPool(vkb.ctx.device, &descriptor_pool_ci,
			  nullptr, &post_desc_pool),
			  "Failed to create descriptor pool");

	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT, SAMPLER_COLOR_BINDING),
	};
	auto set_layout_ci = vk::descriptor_set_layout_CI(
		set_layout_bindings.data(), set_layout_bindings.size());
	vk::check(vkCreateDescriptorSetLayout(vkb.ctx.device, &set_layout_ci,
			  nullptr, &post_desc_layout),
			  "Failed to create descriptor set layout");

	auto set_allocate_info =
		vk::descriptor_set_allocate_info(post_desc_pool, &post_desc_layout, 1);
	vk::check(vkAllocateDescriptorSets(vkb.ctx.device, &set_allocate_info,
			  &post_desc_set),
			  "Failed to allocate descriptor sets");
}

void RayTracer::update_post_desc_set() {
	auto write_desc_set = vk::write_descriptor_set(
		post_desc_set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0,
		&integrator->output_tex.descriptor_image_info);
	vkUpdateDescriptorSets(vkb.ctx.device, 1, &write_desc_set, 0, nullptr);
}

void RayTracer::create_post_pipeline() {
	GraphicsPipelineSettings post_settings;
	VkPipelineLayoutCreateInfo create_info{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

	create_info.setLayoutCount = 1;
	create_info.pSetLayouts = &post_desc_layout;
	create_info.pushConstantRangeCount = 1;
	VkPushConstantRange pc_range = {
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantPost)
	};
	create_info.pPushConstantRanges = &pc_range;
	vkCreatePipelineLayout(vkb.ctx.device, &create_info, nullptr,
						   &post_pipeline_layout);

	post_settings.pipeline_layout = post_pipeline_layout;
	post_settings.render_pass = vkb.ctx.default_render_pass;
	post_settings.shaders = { {"src/shaders/post.vert"},
							 {"src/shaders/post.frag"} };
	for (auto& shader : post_settings.shaders) {
		if (shader.compile()) {
			LUMEN_ERROR("Shader compilation failed");
		}
	}
	post_settings.cull_mode = VK_CULL_MODE_NONE;
	post_settings.enable_tracking = false;
	post_settings.dynamic_state_enables = { VK_DYNAMIC_STATE_VIEWPORT,
										   VK_DYNAMIC_STATE_SCISSOR };
	post_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	post_pipeline->create_gfx_pipeline(post_settings);
}


void RayTracer::init_imgui() {
	VkDescriptorPoolSize pool_sizes[] = {
		{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000} };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	vk::check(vkCreateDescriptorPool(vkb.ctx.device, &pool_info, nullptr,
			  &imgui_pool));
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
		integrator->destroy();
		post_pipeline->cleanup();
		vkb.cleanup();
	}
}
