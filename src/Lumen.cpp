#include "lmhpch.h"
#include "Lumen.h"
#include <glm/glm.hpp>


struct {
	VkPipelineVertexInputStateCreateInfo input_state;
	std::vector<VkVertexInputBindingDescription> binding_descriptions;
	std::vector<VkVertexInputAttributeDescription> attribute_descriptions;
} vertex_descriptions;

struct {
	Buffer triangle;

} vertex_buffers;


struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
};

const std::vector<Vertex> vertices = {
	{{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
	{{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
	{{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}}
};


std::unique_ptr<DefaultPipeline> demo_pipeline;



std::vector<VkDynamicState> dynamic_state_enables = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
};
Lumen::Lumen(int width, int height, bool fullscreen, bool debug) :
	VulkanBase(width, height, fullscreen, debug) {
	shaders = {
	{"src/shaders/triangle.vert"},
	{"src/shaders/triangle.frag"}
	};

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

	if (vkCreateRenderPass(device, &render_pass_CI, nullptr, &render_pass) != VK_SUCCESS) {
		LUMEN_ERROR("failed to create render pass!");
	}
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

	if (demo_pipeline) {
		demo_pipeline->cleanup();
	}


	demo_pipeline = std::make_unique<DefaultPipeline>(
		device,
		vertex_descriptions.input_state,
		shaders,
		dynamic_state_enables,
		render_pass
		);

}

void Lumen::prepare_vertex_buffers() {

	VulkanBase::create_buffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		vertex_buffers.triangle,
		sizeof(vertices[0]) * vertices.size(),
		(void*)vertices.data()
	);

}

void Lumen::build_command_buffers() {

	for (size_t i = 0; i < command_buffers.size(); i++) {
		VkCommandBufferBeginInfo begin_info = vks::command_buffer_begin_info();
		if (vkBeginCommandBuffer(command_buffers[i], &begin_info) != VK_SUCCESS) {
			LUMEN_ERROR("failed to begin recording command buffer!");
		}

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

		vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, demo_pipeline->handle);
		VkBuffer vert_buffers[] = { vertex_buffers.triangle.handle };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(command_buffers[i], 0, 1, vert_buffers, offsets);

		vkCmdDraw(command_buffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(command_buffers[i]);

		if (vkEndCommandBuffer(command_buffers[i]) != VK_SUCCESS) {
			LUMEN_ERROR("Failed to record command buffer");
		}
	}


}

void Lumen::prepare_render() {
	prepare_vertex_buffers();
	build_command_buffers();
}


void Lumen::run() {

	setup_vertex_descriptions();
	VulkanBase::init();
	prepare_render();
	VulkanBase::render_loop();
}

Lumen::~Lumen() {
	vertex_buffers.triangle.destroy();
	demo_pipeline->cleanup();
	VulkanBase::cleanup();

}



