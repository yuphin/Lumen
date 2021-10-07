#include "LumenPCH.h"
#include "BoxScene.h"
#include "Framework/Utils.h"
#include "Framework/CommandBuffer.h"

BoxScene* BoxScene::instance = nullptr;
BoxScene::BoxScene(int width, int height, bool debug) :
	Scene(width, height, debug), resources() {
	this->instance = this;
}

static void fb_resize_callback(GLFWwindow* window, int width, int height) {
	auto app = reinterpret_cast<BoxScene*>(glfwGetWindowUserPointer(window));
	app->resized = true;
}

void BoxScene::init(Window* window) {
	this->window = window;
	vkb.ctx.window_ptr = window->get_window_ptr();
	glfwSetFramebufferSizeCallback(vkb.ctx.window_ptr, fb_resize_callback);
	vkb.create_instance();
	if (vkb.enable_validation_layers) {
		vkb.setup_debug_messenger();
	}
	vkb.create_surface();
	vkb.pick_physical_device();
	vkb.create_logical_device();
	vkb.create_swapchain();
	create_default_render_pass(vkb.ctx);
	vkb.create_framebuffers(vkb.ctx.render_pass);
	vkb.create_command_pool();
	vkb.create_command_buffers();
	vkb.create_sync_primitives();
	init_resources();
	initialized = true;
	auto cam_ptr = camera.get();
	auto instance = this;
	window->add_mouse_move_callback([window, cam_ptr](double delta_x, double delta_y) {
		if (window->is_mouse_held(MouseAction::LEFT)) {
			cam_ptr->rotate(0.05 * delta_y, -0.05 * delta_x, 0);
		}
	});
	init_imgui();
}

void BoxScene::init_resources() {
	constexpr int VERTEX_BINDING_ID = 0;
	camera = std::unique_ptr<PerspectiveCamera>(
		new PerspectiveCamera(45.0f, 0.1f, 1000.0f, (float)width / height,
		glm::vec3(1.25, 1.5, 6.5))
		);
	loader.init(&vkb.ctx);
	resources.cube_material.base_color_idx = 0;
	resources.cube_material.base_color_factor = glm::vec4(0.5f, 0.2f, 0.1f, 1.0f);
	loader.create_cube_model(resources.cube_material, cube_model);
	resources.cube_pipeline_settings.binding_desc = {
	vk::vertex_input_binding_description(
			VERTEX_BINDING_ID,
			sizeof(GLTFModel::Vertex),
			VK_VERTEX_INPUT_RATE_VERTEX
		)
	};
	resources.cube_pipeline_settings.attribute_desc = {
		vk::vertex_input_attribute_description(
			VERTEX_BINDING_ID,
			0,
			VK_FORMAT_R32G32_SFLOAT,
			0
		),
			vk::vertex_input_attribute_description(
			VERTEX_BINDING_ID,
			1,
			VK_FORMAT_R32G32_SFLOAT,
			sizeof(float) * 2
		),

		vk::vertex_input_attribute_description(
			VERTEX_BINDING_ID,
			2,
			VK_FORMAT_R32G32B32_SFLOAT,
			sizeof(float) * 4
		),
		vk::vertex_input_attribute_description(
			VERTEX_BINDING_ID,
			3,
			VK_FORMAT_R32G32B32_SFLOAT,
			sizeof(float) * 7
		),
		vk::vertex_input_attribute_description(
			VERTEX_BINDING_ID,
			4,
			VK_FORMAT_R32G32B32_SFLOAT,
			sizeof(float) * 10
		),
		vk::vertex_input_attribute_description(
			VERTEX_BINDING_ID,
			5,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			sizeof(float) * 13
		),
	};
	resources.cube_pipeline_settings.dynamic_state_enables = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	resources.cube_pipeline_settings.shaders = {
		{"src/Shaders/cube.vert"},
		{"src/Shaders/cube.frag"}
	};
	for (auto& shader : resources.cube_pipeline_settings.shaders) {
		if (shader.compile()) {
			LUMEN_ERROR("Shader compilation failed");
		}
	}
	resources.cube_pipeline_settings.name = "Cube Pipeline";
	resources.cube_pipeline_settings.render_pass = vkb.ctx.render_pass;
	resources.cube_pipeline_settings.bound_models = std::span<Model*>{ loader.models };
	resources.cube_pipeline_settings.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	resources.cube_pipeline_settings.enable_tracking = true;
	loader.num_textures = 1;
	loader.num_materials = 1;
	resources.material_render_func = [this](GLTFModel::Material* primitive_material,
											const VkCommandBuffer& cmd_buffer,
											const VkPipelineLayout& pipeline_layout,
											size_t cb_index) {
		std::array<VkDescriptorSet, 1> descriptor_sets{
			this->resources.uniform_descriptor_sets[cb_index]
		};
		resources.material_push_const.base_color_set = primitive_material->uv_sets.base_color != 0xFF ?
			primitive_material->uv_sets.base_color : -1;
		resources.material_push_const.base_color_factor = primitive_material->base_color_factor;
		vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, resources.cube_pipeline->handle);
		vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
								pipeline_layout, 0, static_cast<uint32_t>(descriptor_sets.size()),
								descriptor_sets.data(), 0, nullptr
		);
		vkCmdPushConstants(cmd_buffer, pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT,
						   0, sizeof(SceneResources::MaterialPushConst), &resources.material_push_const
		);
	};
	this->prepare_buffers();
	this->prepare_descriptors();

	resources.cube_pipeline_settings.pipeline_layout = resources.pipeline_layout;
	resources.cube_pipeline_settings.pipeline = {};


	resources.cube_pipeline = std::make_unique<Pipeline>(vkb.ctx.device);
	resources.cube_pipeline->create_pipeline(resources.cube_pipeline_settings);
	resources.cube_pipeline->track_for_changes();

}

double BoxScene::draw_frame() {
	auto t_begin = std::chrono::high_resolution_clock::now();
	vk::check(vkWaitForFences(vkb.ctx.device, 1, &vkb.in_flight_fences[vkb.current_frame], VK_TRUE, 1000000000),
			  "Timeout");
	uint32_t image_idx;
	VkResult result = vkAcquireNextImageKHR(
		vkb.ctx.device,
		vkb.ctx.swapchain,
		UINT64_MAX,
		vkb.image_available_sem[vkb.current_frame],
		VK_NULL_HANDLE,
		&image_idx);
	vk::check(vkResetCommandBuffer(vkb.ctx.command_buffers[image_idx], 0));
	if (result == VK_NOT_READY) {
		auto t_end = std::chrono::high_resolution_clock::now();
		auto t_diff = std::chrono::duration<double, std::milli>(t_end - t_begin).count();
		return t_diff;
	}
	if (EventHandler::consume_event(LumenEvent::EVENT_SHADER_RELOAD)) {
		// We don't want any command buffers in flight, might change in the future
		vkDeviceWaitIdle(vkb.ctx.device);
		for (auto& old_pipeline : EventHandler::obsolete_pipelines) {
			vkDestroyPipeline(vkb.ctx.device, old_pipeline, nullptr);
		}
		EventHandler::obsolete_pipelines.clear();
		vkb.create_command_buffers();
		auto t_end = std::chrono::high_resolution_clock::now();
		auto t_diff = std::chrono::duration<double, std::milli>(t_end - t_begin).count();
		return t_diff;
	}

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		// Window resize
		vkb.recreate_swap_chain(create_default_render_pass, vkb.ctx);
		auto t_end = std::chrono::high_resolution_clock::now();
		auto t_diff = std::chrono::duration<double, std::milli>(t_end - t_begin).count();
		return t_diff;
	} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		LUMEN_ERROR("Failed to acquire new swap chain image");
	}

	if (vkb.images_in_flight[image_idx] != VK_NULL_HANDLE) {
		vkWaitForFences(vkb.ctx.device, 1, &vkb.images_in_flight[image_idx], VK_TRUE, UINT64_MAX);
	}
	vkb.images_in_flight[image_idx] = vkb.in_flight_fences[vkb.current_frame];

	render(image_idx);

	VkSubmitInfo submit_info = vk::submit_info();
	VkSemaphore wait_semaphores[] = { vkb.image_available_sem[vkb.current_frame] };
	VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vkb.ctx.command_buffers[image_idx];

	VkSemaphore signal_semaphores[] = { vkb.render_finished_sem[vkb.current_frame] };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;

	vkResetFences(vkb.ctx.device, 1, &vkb.in_flight_fences[vkb.current_frame]);

	vk::check(vkQueueSubmit(vkb.ctx.gfx_queue, 1, &submit_info, vkb.in_flight_fences[vkb.current_frame]),
			  "Failed to submit draw command buffer"
	);
	VkPresentInfoKHR present_info{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;

	VkSwapchainKHR swapchains[] = { vkb.ctx.swapchain };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swapchains;

	present_info.pImageIndices = &image_idx;

	result = vkQueuePresentKHR(vkb.ctx.present_queue, &present_info);

	if (result == VK_ERROR_OUT_OF_DATE_KHR ||
		result == VK_SUBOPTIMAL_KHR ||
		resized) {
		resized = false;
		vkb.recreate_swap_chain(create_default_render_pass, vkb.ctx);
	} else if (result != VK_SUCCESS) {
		LUMEN_ERROR("Failed to present swap chain image");
	}

	vkb.current_frame = (vkb.current_frame + 1) % vkb.MAX_FRAMES_IN_FLIGHT;
	auto t_end = std::chrono::high_resolution_clock::now();
	auto t_diff = std::chrono::duration<double, std::milli>(t_end - t_begin).count();
	return t_diff;
}

void BoxScene::prepare_descriptors() {
	constexpr int UNIFORM_BUFFER_BINDING = 0;
	constexpr int SAMPLER_COLOR_BINDING = 0;

	std::vector<VkDescriptorPoolSize> pool_sizes = {
	vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,vkb.ctx.swapchain_images.size()),
	vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<size_t>(loader.num_materials))
	};
	auto descriptor_pool_ci = vk::descriptor_pool_CI(
		pool_sizes.size(),
		pool_sizes.data(),
		loader.num_textures + vkb.ctx.swapchain_images.size()
	);

	vk::check(vkCreateDescriptorPool(vkb.ctx.device, &descriptor_pool_ci, nullptr, &vkb.ctx.descriptor_pool),
			  "Failed to create descriptor pool");

	// Uniform buffer descriptors
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
		vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT,
			UNIFORM_BUFFER_BINDING),
	};
	auto set_layout_ci = vk::descriptor_set_layout_CI(
		set_layout_bindings.data(),
		set_layout_bindings.size()
	);
	vk::check(vkCreateDescriptorSetLayout(vkb.ctx.device, &set_layout_ci, nullptr, &resources.uniform_set_layout),
			  "Failed to create descriptor set layout");
	// Sampler descriptors
	set_layout_bindings = {
	vk::descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			SAMPLER_COLOR_BINDING),
	};

	set_layout_ci.pBindings = set_layout_bindings.data();
	set_layout_ci.bindingCount = static_cast<uint32_t>(set_layout_bindings.size());
	vk::check(vkCreateDescriptorSetLayout(vkb.ctx.device, &set_layout_ci, nullptr, &resources.scene_set_layout),
			  "Failed to create descriptor set layout");

	std::array<VkDescriptorSetLayout, 1> set_layouts = { resources.uniform_set_layout/*, resources.scene_set_layout*/ };
	auto pipeline_layout_ci = vk::pipeline_layout_CI(
		set_layouts.data(),
		static_cast<uint32_t>(set_layouts.size())
	);

	// Need to specify push constant range
	auto push_constant_range = vk::push_constant_range(
		VK_SHADER_STAGE_FRAGMENT_BIT,
		sizeof(SceneResources::MaterialPushConst), 0
	);
	pipeline_layout_ci.pushConstantRangeCount = 1;
	pipeline_layout_ci.pPushConstantRanges = &push_constant_range;
	vk::check(vkCreatePipelineLayout(vkb.ctx.device, &pipeline_layout_ci, nullptr, &resources.pipeline_layout),
			  "Failed to create pipeline layout");

	// Descriptor sets for the uniform matrices(For each swapchain image)
	auto set_layout_vec = std::vector<VkDescriptorSetLayout>(vkb.ctx.swapchain_images.size(), resources.uniform_set_layout);
	auto set_allocate_info = vk::descriptor_set_allocate_info(
		vkb.ctx.descriptor_pool,
		set_layout_vec.data(),
		vkb.ctx.swapchain_images.size()
	);
	resources.uniform_descriptor_sets.resize(vkb.ctx.swapchain_images.size());
	vk::check(vkAllocateDescriptorSets(vkb.ctx.device, &set_allocate_info, resources.uniform_descriptor_sets.data()),
			  "Failed to allocate descriptor sets");
	for (auto i = 0; i < resources.uniform_descriptor_sets.size(); i++) {
		std::vector<VkWriteDescriptorSet> write_descriptor_sets{
			vk::write_descriptor_set(
				resources.uniform_descriptor_sets[i],
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				UNIFORM_BUFFER_BINDING,
				&resources.vertex_buffers.scene_ubo.descriptor
			)
		};
		vkUpdateDescriptorSets(vkb.ctx.device,
							   static_cast<uint32_t>(write_descriptor_sets.size()),
							   write_descriptor_sets.data(), 0, nullptr);
	}

	// Descriptor sets for materials
	for (auto model : loader.models) {
		for (auto& material : model->materials) {
			/*auto set_allocate_info = vks::descriptor_set_allocate_info(
				descriptor_pool,
				&resources.scene_set_layout,
				1
			);
			vkAllocateDescriptorSets(device, &set_allocate_info, &material.descriptor_set);
			VkDescriptorImageInfo color_map = model.get_texture_descriptor(material.base_color_idx);
			std::vector<VkWriteDescriptorSet> write_descriptor_sets{
						vks::write_descriptor_set(
							material.descriptor_set,
							VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							SAMPLER_COLOR_BINDING,
							&color_map
						),
			};
			vkUpdateDescriptorSets(device,
				static_cast<uint32_t>(write_descriptor_sets.size()),
				write_descriptor_sets.data(), 0, nullptr);*/
		}
	}
}

void BoxScene::prepare_buffers() {
	resources.vertex_buffers.scene_ubo.create(
		&vkb.ctx,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		sizeof(SceneUBO));
	update_buffers();
}

void BoxScene::prepare_descriptor_pool() {
	std::vector<VkDescriptorPoolSize> pool_sizes = {
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,vkb.ctx.swapchain_images.size()),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, vkb.ctx.swapchain_images.size())
	};
	auto descriptor_pool_ci = vk::descriptor_pool_CI(
		pool_sizes.size(),
		pool_sizes.data(),
		vkb.ctx.swapchain_images.size()
	);
	vk::check(vkCreateDescriptorPool(vkb.ctx.device, &descriptor_pool_ci, nullptr, &vkb.ctx.descriptor_pool),
			  "Failed to create descriptor pool");
}

void BoxScene::update_buffers() {
	SceneUBO scene_ubo{};
	camera->update_view_matrix();
	scene_ubo.view = camera->view;
	scene_ubo.projection = camera->projection;
	scene_ubo.view_pos = glm::vec4(camera->position, 1);
	memcpy(resources.vertex_buffers.scene_ubo.data, &scene_ubo, sizeof(scene_ubo));
}

void BoxScene::init_imgui() {
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = std::size(pool_sizes);
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
	init_info.Queue = vkb.ctx.gfx_queue;
	init_info.DescriptorPool = imgui_pool;
	init_info.MinImageCount = 2;
	init_info.ImageCount = 2;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&init_info, vkb.ctx.render_pass);

	CommandBuffer cmd(&vkb.ctx, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	ImGui_ImplVulkan_CreateFontsTexture(cmd.handle);
	cmd.submit(vkb.ctx.gfx_queue);

	ImGui_ImplVulkan_DestroyFontUploadObjects();

}

std::string BoxScene::get_asset_path() const {
	return std::string("assets/");
}

void BoxScene::render(uint32_t i) {

	VkCommandBufferBeginInfo begin_info = vk::command_buffer_begin_info(
		VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	);
	vk::check(vkBeginCommandBuffer(vkb.ctx.command_buffers[i], &begin_info));

	VkRenderPassBeginInfo render_pass_info = vk::render_pass_begin_info();
	render_pass_info.renderPass = vkb.ctx.render_pass;
	render_pass_info.framebuffer = vkb.ctx.swapchain_framebuffers[i];
	render_pass_info.renderArea.offset = { 0, 0 };
	render_pass_info.renderArea.extent = vkb.ctx.swapchain_extent;

	VkClearValue clear_color = { 0.25f, 0.25f, 0.25f, 1.0f };
	render_pass_info.clearValueCount = 1;
	render_pass_info.pClearValues = &clear_color;

	vkCmdBeginRenderPass(vkb.ctx.command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport viewport = vk::viewport((float)width, (float)height, 0.0f, 1.0f);
	vkCmdSetViewport(vkb.ctx.command_buffers[i], 0, 1, &viewport);

	VkRect2D scissor = vk::rect2D(width, height, 0, 0);
	vkCmdSetScissor(vkb.ctx.command_buffers[i], 0, 1, &scissor);

	vkCmdBindDescriptorSets(vkb.ctx.command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
							resources.pipeline_layout, 0, 1, &resources.uniform_descriptor_sets[i], 0, nullptr
	);
	for (auto& model : loader.models) {

		model->draw(vkb.ctx.command_buffers[i], resources.pipeline_layout, i, resources.material_render_func);
	}


	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	bool open = true;
	ImGui::ShowDemoWindow(&open);
	ImGui::Render();
	ImDrawData* draw_data = ImGui::GetDrawData();
	const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
	if (is_minimized) {
		return;
	}

	ImGui_ImplVulkan_RenderDrawData(draw_data, vkb.ctx.command_buffers[i]);

	vkCmdEndRenderPass(vkb.ctx.command_buffers[i]);

	vk::check(vkEndCommandBuffer(vkb.ctx.command_buffers[i]),
			  "Failed to record command buffer"
	);
}

void BoxScene::update() {

	double frame_time = draw_frame();
	frame_time /= 1000.0;
	glm::vec3 translation{};
	float trans_speed = 0.001;
	glm::vec3 front;
	if (window->is_key_held(KeyInput::KEY_LEFT_SHIFT)) {
		trans_speed *= 4;
	}
	front.x = cos(glm::radians(camera->rotation.x)) * sin(glm::radians(camera->rotation.y));
	front.y = sin(glm::radians(camera->rotation.x));
	front.z = cos(glm::radians(camera->rotation.x)) * cos(glm::radians(camera->rotation.y));
	front = glm::normalize(-front);
	if (window->is_key_held(KeyInput::KEY_W)) {
		camera->position += front * trans_speed;
	}
	if (window->is_key_held(KeyInput::KEY_A)) {
		camera->position -= glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * trans_speed;
	}
	if (window->is_key_held(KeyInput::KEY_S)) {
		camera->position -= front * trans_speed;
	}
	if (window->is_key_held(KeyInput::KEY_D)) {
		camera->position += glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f))) * trans_speed;
	}
	if (window->is_key_held(KeyInput::SPACE)) {
		// Right
		auto right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
		auto up = glm::cross(right, front);
		camera->position += up * trans_speed;
	}
	if (window->is_key_held(KeyInput::KEY_LEFT_CONTROL)) {
		auto right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
		auto up = glm::cross(right, front);
		camera->position -= up * trans_speed;
	}
	update_buffers();
}

void BoxScene::cleanup() {
	vkDeviceWaitIdle(vkb.ctx.device);
	resources.vertex_buffers.scene_ubo.destroy();
	vkDestroyDescriptorSetLayout(vkb.ctx.device, resources.scene_set_layout, nullptr);
	vkDestroyDescriptorSetLayout(vkb.ctx.device, resources.uniform_set_layout, nullptr);
	vkDestroyPipelineLayout(vkb.ctx.device, resources.pipeline_layout, nullptr);
	loader.destroy();
	if (resources.cube_pipeline) { resources.cube_pipeline->cleanup(); }
	if (initialized) { vkb.cleanup(); }
}
