#include "lmhpch.h"
#include "Lumen.h"

Lumen::Lumen(int width, int height, bool debug) :
	width(width), height(height), fullscreen(fullscreen), debug(debug),
	VulkanBase(width, height, fullscreen, debug),
	shaders(
		{
			{"src/shaders/triangle.vert"},
			{"src/shaders/triangle.frag"}
		}
	) {
	cam = std::unique_ptr<PerspectiveCamera>(
		new PerspectiveCamera(90.0f, 0.1f, 1000.0f, (float)width / height)
		);
	cam->set_pos(glm::vec3(0.0, 0.0, 1));
	for (auto& shader : shaders) {
		if (shader.compile()) {
			return;
		}
	}
}

void Lumen::create_render_pass() {
	// Simple render pass for a triangle 
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

	vks::check(
		vkCreateRenderPass(device, &render_pass_CI, nullptr, &render_pass),
		"Failed to create render pass!"
	);
}



void Lumen::setup_vertex_descriptions() {
	constexpr int BIND_ID = 0;
	VertexLayout layout = {
		{
		vks::Component::L_POSITION,
		vks::Component::L_COLOR
		}
	};

	// Binding description
	vertex_descriptions.binding_descriptions.resize(1);
	vertex_descriptions.binding_descriptions[0] =
		vks::vertex_input_binding_description(
			BIND_ID,
			layout.stride(),
			VK_VERTEX_INPUT_RATE_VERTEX);

	vertex_descriptions.attribute_descriptions.resize(layout.components.size());
	vertex_descriptions.attribute_descriptions[0] =
		vks::vertex_input_attribute_description(
			BIND_ID,
			0,
			VK_FORMAT_R32G32B32_SFLOAT,
			0
		);
	vertex_descriptions.attribute_descriptions[1] =
		vks::vertex_input_attribute_description(
			BIND_ID,
			1,
			VK_FORMAT_R32G32B32_SFLOAT,
			3 * sizeof(float)
		);

	vertex_descriptions.input_state = vks::pipeline_vertex_input_state_CI();
	vertex_descriptions.input_state.vertexBindingDescriptionCount =
		static_cast<uint32_t>(vertex_descriptions.binding_descriptions.size());

	vertex_descriptions.input_state.pVertexBindingDescriptions =
		vertex_descriptions.binding_descriptions.data();

	vertex_descriptions.input_state.vertexAttributeDescriptionCount =
		static_cast<uint32_t>(vertex_descriptions.attribute_descriptions.size());

	vertex_descriptions.input_state.pVertexAttributeDescriptions =
		vertex_descriptions.attribute_descriptions.data();
}

void Lumen::create_gfx_pipeline() {

	const std::vector<VkDynamicState> dynamic_state_enables = {
				VK_DYNAMIC_STATE_VIEWPORT,
				VK_DYNAMIC_STATE_SCISSOR
	};

	if (demo_pipeline) {
		demo_pipeline->cleanup();
	}

	demo_pipeline = std::make_unique<DefaultPipeline>(
		device,
		vertex_descriptions.input_state,
		shaders,
		dynamic_state_enables,
		render_pass,
		pipeline_layout
		);

}

void Lumen::prepare_buffers() {

	const std::vector<Vertex> vertices = {
	{{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
	{{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
	{{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}
	};

	VulkanBase::create_buffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		vertex_buffers.triangle,
		sizeof(vertices[0]) * vertices.size(),
		(void*)vertices.data()
	);

	VulkanBase::create_buffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		vertex_buffers.scene_ubo,
		sizeof(SceneUBO));
	vertex_buffers.scene_ubo.map_memory();


}

void Lumen::prepare_descriptor_layouts() {
	constexpr int SCENE_LAYOUT_BINDING = 0;
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
			vks::descriptor_set_layout_binding(
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				VK_SHADER_STAGE_VERTEX_BIT,
				SCENE_LAYOUT_BINDING)
	};
	auto set_layout_ci = vks::descriptor_set_layout_CI(
		set_layout_bindings.data(),
		set_layout_bindings.size()
	);
	vks::check(vkCreateDescriptorSetLayout(device, &set_layout_ci, nullptr, &set_layout),
		"Failed to create descriptor set layout");
	auto pipeline_layout_ci = vks::pipeline_layout_CI(
		&set_layout,
		1
	);


	vks::check(vkCreatePipelineLayout(device, &pipeline_layout_ci, nullptr, &pipeline_layout),
		"Failed to create pipeline layout");
}

void Lumen::prepare_descriptor_pool() {
	std::vector<VkDescriptorPoolSize> pool_sizes = {
		vks::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		swapchain_images.size())
	};
	auto descriptor_pool_ci = vks::descriptor_pool_CI(
		pool_sizes.size(),
		pool_sizes.data(),
		swapchain_images.size()
	);

	vks::check(vkCreateDescriptorPool(device, &descriptor_pool_ci, nullptr, &descriptor_pool),
		"Failed to create descriptor pool");

}

void Lumen::prepare_descriptor_sets() {
	std::vector<VkDescriptorSetLayout> layouts(swapchain_images.size(), set_layout);
	auto set_allocate_info = vks::descriptor_set_allocate_info(
		descriptor_pool,
		layouts.data(),
		swapchain_images.size()
	);
	descriptor_sets.resize(swapchain_images.size());
	vks::check(vkAllocateDescriptorSets(device, &set_allocate_info, descriptor_sets.data()),
		"Failed to allocate descriptor sets");
	for (auto i = 0; i < descriptor_sets.size(); i++) {
		std::vector<VkWriteDescriptorSet> write_descriptor_sets{
			vks::write_descriptor_set(
				descriptor_sets[i],
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				0,
				&vertex_buffers.scene_ubo.descriptor
			)
		};
		vkUpdateDescriptorSets(device, 
			static_cast<uint32_t>(write_descriptor_sets.size()), 
			write_descriptor_sets.data(), 0, nullptr);
	}
}

void Lumen::update_buffers() {
	SceneUBO scene_ubo{};
	scene_ubo.model = glm::mat4{ 1 };
	cam->update_view_matrix();
	scene_ubo.view = cam->view;
	scene_ubo.projection = cam->proj;
	memcpy(vertex_buffers.scene_ubo.data, &scene_ubo, sizeof(scene_ubo));
}


void Lumen::build_command_buffers() {

	for (size_t i = 0; i < command_buffers.size(); i++) {
		VkCommandBufferBeginInfo begin_info = vks::command_buffer_begin_info();
		vks::check(
			vkBeginCommandBuffer(command_buffers[i], &begin_info),
			"Failed to begin recording command buffer!"
		);

		VkRenderPassBeginInfo render_pass_info = vks::render_pass_begin_info();
		render_pass_info.renderPass = render_pass;
		render_pass_info.framebuffer = swapchain_framebuffers[i];
		render_pass_info.renderArea.offset = { 0, 0 };
		render_pass_info.renderArea.extent = swapchain_extent;

		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		render_pass_info.clearValueCount = 1;
		render_pass_info.pClearValues = &clearColor;

		vkCmdBeginRenderPass(command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
		VkViewport viewport = vks::viewport((float)width, (float)height, 0.0f, 1.0f);
		vkCmdSetViewport(command_buffers[i], 0, 1, &viewport);

		VkRect2D scissor = vks::rect2D(width, height, 0, 0);
		vkCmdSetScissor(command_buffers[i], 0, 1, &scissor);

		vkCmdBindDescriptorSets(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_sets[i], 0, nullptr);
		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, demo_pipeline->handle);
		VkBuffer vert_buffers[] = { vertex_buffers.triangle.handle };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vert_buffers, offsets);

		vkCmdDraw(command_buffers[i], 3, 1, 0, 0);
		vkCmdEndRenderPass(command_buffers[i]);

		vks::check(
			vkEndCommandBuffer(command_buffers[i]),
			"Failed to record command buffer"
		);
	}


}

void Lumen::prepare_render() {
	prepare_buffers();
	prepare_descriptor_layouts();
	prepare_descriptor_pool();
	prepare_descriptor_sets();
	create_gfx_pipeline();
	build_command_buffers();
}


void Lumen::init(GLFWwindow* window_ptr) {
	this->window = window_ptr;
	setup_vertex_descriptions();
	VulkanBase::init(window);
	prepare_render();
}

void Lumen::update() {
	update_buffers();
	draw_frame();
}

Lumen::~Lumen() {
	vkDeviceWaitIdle(device);
	vertex_buffers.triangle.destroy();
	vertex_buffers.scene_ubo.destroy();
	vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
	if (demo_pipeline) { demo_pipeline->cleanup(); }
	if (initialized) { VulkanBase::cleanup(); }

}



