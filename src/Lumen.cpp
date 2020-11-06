#include "LumenPCH.h"
#include "Lumen.h"
#include "Framework/Utils.h"
Lumen* Lumen::instance = nullptr;
Lumen::Lumen(int width, int height, bool debug) :
	width(width), height(height), fullscreen(fullscreen), debug(debug),
	VulkanBase(width, height, fullscreen, debug), resources() {
	this->instance = this;
}

void Lumen::init(GLFWwindow* window_ptr) {
	this->window = window_ptr;
	VulkanBase::init(window);
	this->lumen_ctx = {
		&this->device,
		&this->gfx_queue,
		&this->present_queue,
		&this->compute_queue,
		&this->supported_features,
		&this->device_properties,
		&this->memory_properties,
		&this->command_pool,
		&this->physical_device
	};
	this->init_resources();
	this->build_command_buffers();
}


void Lumen::init_resources() {
	constexpr int VERTEX_BINDING_ID = 0;
	camera = std::unique_ptr<PerspectiveCamera>(
		new PerspectiveCamera(45.0f, 0.1f, 1000.0f, (float) width / height,
		glm::vec3(1.25, 1.5, 2.5))
		);
	camera->rotate(glm::vec3(-10.5f, 20.4, 0));
	scene.init(&this->lumen_ctx);
	resources.cube_material.base_color_idx = 0;
	resources.cube_material.base_color_factor = glm::vec4(0.5f, 0.2f, 0.1f, 1.0f);
	scene.add_cube_model(resources.cube_material);
	resources.cube_pipeline_settings.binding_desc = {
	vk::vertex_input_binding_description(
			VERTEX_BINDING_ID,
			sizeof(Model::Vertex),
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
	for(auto& shader : resources.cube_pipeline_settings.shaders) {
		if(shader.compile()) {
			LUMEN_ERROR("Shader compilation failed");
		}
	}
	resources.cube_pipeline_settings.name = "Cube Pipeline";
	resources.cube_pipeline_settings.render_pass = this->render_pass;
	resources.cube_pipeline_settings.bound_models = std::span<Model>{ scene.models };
	resources.cube_pipeline_settings.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	resources.cube_pipeline_settings.enable_tracking = false;
	scene.num_textures = 1;
	scene.num_materials = 1;
	resources.material_render_func = [this](Model::Material* primitive_material,
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

	resources.cube_pipeline = std::make_unique<Pipeline>(this->device);
	resources.cube_pipeline->create_pipeline(resources.cube_pipeline_settings);

}

void Lumen::prepare_descriptors() {
	constexpr int UNIFORM_BUFFER_BINDING = 0;
	constexpr int SAMPLER_COLOR_BINDING = 0;

	std::vector<VkDescriptorPoolSize> pool_sizes = {
	vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,swapchain_images.size()),
	vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<size_t>(scene.num_materials))
	};
	auto descriptor_pool_ci = vk::descriptor_pool_CI(
		pool_sizes.size(),
		pool_sizes.data(),
		scene.num_textures + swapchain_images.size()
	);

	vk::check(vkCreateDescriptorPool(device, &descriptor_pool_ci, nullptr, &descriptor_pool),
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
	vk::check(vkCreateDescriptorSetLayout(device, &set_layout_ci, nullptr, &resources.uniform_set_layout),
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
	vk::check(vkCreateDescriptorSetLayout(device, &set_layout_ci, nullptr, &resources.scene_set_layout),
			  "Failed to create descriptor set layout");

	std::array<VkDescriptorSetLayout, 2> set_layouts = { resources.uniform_set_layout, resources.scene_set_layout };
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
	vk::check(vkCreatePipelineLayout(device, &pipeline_layout_ci, nullptr, &resources.pipeline_layout),
			  "Failed to create pipeline layout");

	// Descriptor sets for the uniform matrices(For each swapchain image)
	auto set_layout_vec = std::vector<VkDescriptorSetLayout>(swapchain_images.size(), resources.uniform_set_layout);
	auto set_allocate_info = vk::descriptor_set_allocate_info(
		descriptor_pool,
		set_layout_vec.data(),
		swapchain_images.size()
	);
	resources.uniform_descriptor_sets.resize(swapchain_images.size());
	vk::check(vkAllocateDescriptorSets(device, &set_allocate_info, resources.uniform_descriptor_sets.data()),
			  "Failed to allocate descriptor sets");
	for(auto i = 0; i < resources.uniform_descriptor_sets.size(); i++) {
		std::vector<VkWriteDescriptorSet> write_descriptor_sets{
			vk::write_descriptor_set(
				resources.uniform_descriptor_sets[i],
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				UNIFORM_BUFFER_BINDING,
				&resources.vertex_buffers.scene_ubo.descriptor
			)
		};
		vkUpdateDescriptorSets(device,
							   static_cast<uint32_t>(write_descriptor_sets.size()),
							   write_descriptor_sets.data(), 0, nullptr);
	}

	// Descriptor sets for materials
	for(auto& model : scene.models) {
		for(auto& material : model.materials) {
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

void Lumen::create_render_pass() {
	// Simple render pass 
	VkAttachmentDescription color_attachment{};
	color_attachment.format = swapchain_image_format;
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref{};
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo render_pass_CI{};
	render_pass_CI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_CI.attachmentCount = 1;
	render_pass_CI.pAttachments = &color_attachment;
	render_pass_CI.subpassCount = 1;
	render_pass_CI.pSubpasses = &subpass;
	render_pass_CI.dependencyCount = 1;
	render_pass_CI.pDependencies = &dependency;

	vk::check(vkCreateRenderPass(device, &render_pass_CI, nullptr, &this->render_pass),
			  "Failed to create render pass!"
	);
}

void Lumen::prepare_buffers() {

	resources.vertex_buffers.scene_ubo.create(
		&lumen_ctx,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		sizeof(SceneUBO));
	resources.vertex_buffers.scene_ubo.map_memory();
	update_buffers();
}

void Lumen::prepare_descriptor_pool() {
	std::vector<VkDescriptorPoolSize> pool_sizes = {
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,swapchain_images.size()),
		vk::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, swapchain_images.size())
	};
	auto descriptor_pool_ci = vk::descriptor_pool_CI(
		pool_sizes.size(),
		pool_sizes.data(),
		swapchain_images.size()
	);

	vk::check(vkCreateDescriptorPool(device, &descriptor_pool_ci, nullptr, &descriptor_pool),
			  "Failed to create descriptor pool");
}

void Lumen::update_buffers() {
	SceneUBO scene_ubo{};
	camera->update_view_matrix();
	scene_ubo.view = camera->view;
	scene_ubo.projection = camera->projection;
	scene_ubo.view_pos = glm::vec4(camera->position, 1);
	memcpy(resources.vertex_buffers.scene_ubo.data, &scene_ubo, sizeof(scene_ubo));
}

std::string Lumen::get_asset_path() const {
	return std::string("assets/");
}

void Lumen::build_command_buffers() {

	for(size_t i = 0; i < command_buffers.size(); i++) {
		VkCommandBufferBeginInfo begin_info = vk::command_buffer_begin_info();
		vk::check(vkBeginCommandBuffer(command_buffers[i], &begin_info),
				  "Failed to begin recording command buffer!"
		);

		VkRenderPassBeginInfo render_pass_info = vk::render_pass_begin_info();
		render_pass_info.renderPass = render_pass;
		render_pass_info.framebuffer = swapchain_framebuffers[i];
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = swapchain_extent;

		VkClearValue clear_color = { 0.25f, 0.25f, 0.25f, 1.0f };
		render_pass_info.clearValueCount = 1;
		render_pass_info.pClearValues = &clear_color;

		vkCmdBeginRenderPass(command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		VkViewport viewport = vk::viewport((float) width, (float) height, 0.0f, 1.0f);
		vkCmdSetViewport(command_buffers[i], 0, 1, &viewport);

		VkRect2D scissor = vk::rect2D(width, height, 0, 0);
		vkCmdSetScissor(command_buffers[i], 0, 1, &scissor);

		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
								resources.pipeline_layout, 0, 1, &resources.uniform_descriptor_sets[i], 0, nullptr
		);
		for(auto& model : scene.models) {

			model.draw(command_buffers[i], resources.pipeline_layout, i, resources.material_render_func);
		}
		vkCmdEndRenderPass(command_buffers[i]);

		vk::check(vkEndCommandBuffer(command_buffers[i]),
				  "Failed to record command buffer"
		);
	}
}

void Lumen::update() {
	update_buffers();
	draw_frame();
}

Lumen::~Lumen() {
	vkDeviceWaitIdle(device);
	resources.vertex_buffers.scene_ubo.destroy();
	vkDestroyDescriptorSetLayout(device, resources.scene_set_layout, nullptr);
	vkDestroyDescriptorSetLayout(device, resources.uniform_set_layout, nullptr);
	vkDestroyPipelineLayout(device, resources.pipeline_layout, nullptr);
	scene.destroy();
	if(resources.cube_pipeline) { resources.cube_pipeline->cleanup(); }
	if(initialized) { VulkanBase::cleanup(); }
}



