#include "ImGuiRenderer.h"
#include "Buffer.h"
#include "PersistentResourceManager.h"
#include "RenderGraph.h"
#include "Texture.h"
#include "Window.h"

struct ImGuiPushConstants {
	lm::vec2 scale;
	lm::vec2 translate;
};

static u64 grow_capacity(u64 capacity, u64 required, u64 minimum) {
	capacity = lm::max(capacity, minimum);
	while (capacity < required) {
		capacity += capacity / 2;
	}
	return capacity;
}

static void ensure_buffer(vk::Buffer*& buffer, u64& capacity, u64 required, VkBufferUsageFlags usage,
						  const lm::String& name) {
	if (required <= capacity) return;
	prm::remove(buffer);
	capacity = grow_capacity(capacity, required, KB(64));
	buffer = prm::get_buffer({
		.name = name,
		.usage = usage,
		.memory_type = vk::BUFFER_TYPE_CPU_TO_GPU,
		.size = capacity,
		.create_mapped = true,
	});
}

static void upload_draw_data(ImGuiFrameResources* frame, ImDrawData* draw_data) {
	const u64 vertex_size = u64(draw_data->TotalVtxCount) * sizeof(ImDrawVert);
	const u64 index_size = u64(draw_data->TotalIdxCount) * sizeof(ImDrawIdx);

	ensure_buffer(frame->vertex_buffer, frame->vertex_capacity, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				  CSTR("ImGui Vertex Buffer"));
	ensure_buffer(frame->index_buffer, frame->index_capacity, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				  CSTR("ImGui Index Buffer"));

	ImDrawVert* vertex_dst = (ImDrawVert*)vk::buffer_map(frame->vertex_buffer);
	ImDrawIdx* index_dst = (ImDrawIdx*)vk::buffer_map(frame->index_buffer);
	for (i32 i = 0; i < draw_data->CmdListsCount; ++i) {
		const ImDrawList* draw_list = draw_data->CmdLists[i];
		memcpy(vertex_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(index_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vertex_dst += draw_list->VtxBuffer.Size;
		index_dst += draw_list->IdxBuffer.Size;
	}
	vk::buffer_flush(frame->vertex_buffer, 0, vertex_size);
	vk::buffer_flush(frame->index_buffer, 0, index_size);
	vk::buffer_unmap(frame->vertex_buffer);
	vk::buffer_unmap(frame->index_buffer);
}

static void render_draw_data(VkCommandBuffer cmd, const lm::RenderPass& pass) {
	ImDrawData* draw_data = ImGui::GetDrawData();
	const i32 fb_width = (i32)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
	const i32 fb_height = (i32)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
	if (fb_width <= 0 || fb_height <= 0) return;

	const ImVec2 clip_offset = draw_data->DisplayPos;
	const ImVec2 clip_scale = draw_data->FramebufferScale;
	i32 global_vertex_offset = 0;
	i32 global_index_offset = 0;

	for (i32 list_idx = 0; list_idx < draw_data->CmdListsCount; ++list_idx) {
		const ImDrawList* draw_list = draw_data->CmdLists[list_idx];
		for (i32 cmd_idx = 0; cmd_idx < draw_list->CmdBuffer.Size; ++cmd_idx) {
			const ImDrawCmd* draw_cmd = &draw_list->CmdBuffer[cmd_idx];
			if (draw_cmd->UserCallback) {
				if (draw_cmd->UserCallback == ImDrawCallback_ResetRenderState) {
					pass.bind_graphics_state(cmd);
				} else {
					draw_cmd->UserCallback(draw_list, draw_cmd);
				}
				continue;
			}

			ImVec2 clip_min((draw_cmd->ClipRect.x - clip_offset.x) * clip_scale.x,
							(draw_cmd->ClipRect.y - clip_offset.y) * clip_scale.y);
			ImVec2 clip_max((draw_cmd->ClipRect.z - clip_offset.x) * clip_scale.x,
							(draw_cmd->ClipRect.w - clip_offset.y) * clip_scale.y);
			clip_min.x = lm::max(clip_min.x, 0.0f);
			clip_min.y = lm::max(clip_min.y, 0.0f);
			clip_max.x = lm::min(clip_max.x, (f32)fb_width);
			clip_max.y = lm::min(clip_max.y, (f32)fb_height);
			if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) continue;

			VkRect2D scissor = {
				.offset = {(i32)clip_min.x, (i32)clip_min.y},
				.extent = {(u32)(clip_max.x - clip_min.x), (u32)(clip_max.y - clip_min.y)},
			};
			vkCmdSetScissor(cmd, 0, 1, &scissor);

			vk::Texture* texture = (vk::Texture*)draw_cmd->GetTexID();
			LUMEN_ASSERT(texture, "ImGui draw command has no texture");
			if (!texture) continue;
			vk::DescriptorInfo descriptor(vk::texture_descriptor(texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
			pass.push_descriptors(cmd, &descriptor);
			vkCmdDrawIndexed(cmd, draw_cmd->ElemCount, 1, draw_cmd->IdxOffset + global_index_offset,
							 draw_cmd->VtxOffset + global_vertex_offset, 0);
		}
		global_index_offset += draw_list->IdxBuffer.Size;
		global_vertex_offset += draw_list->VtxBuffer.Size;
	}
}

namespace imgui_renderer {

void init(ImGuiRenderer* renderer) {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.BackendRendererName = "lumen";
	io.BackendRendererUserData = renderer;
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
	Window::imgui_init();

	u8* pixels = nullptr;
	i32 width = 0;
	i32 height = 0;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
	renderer->font_texture = prm::get_texture({
		.name = CSTR("ImGui Font Atlas"),
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.dimensions = {(u32)width, (u32)height, 1},
		.format = VK_FORMAT_R8G8B8A8_UNORM,
		.data = {.data = pixels, .size = u64(width) * u64(height) * 4},
	});
	io.Fonts->SetTexID((ImTextureID)renderer->font_texture);
}

void new_frame(ImGuiRenderer*) {
	Window::imgui_new_frame();
	ImGui::NewFrame();
}

void add_pass(ImGuiRenderer* renderer, vk::Texture* output) {
	ImGui::Render();
	ImDrawData* draw_data = ImGui::GetDrawData();
	const i32 fb_width = (i32)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
	const i32 fb_height = (i32)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
	if (fb_width <= 0 || fb_height <= 0 || draw_data->TotalVtxCount == 0) return;

	ImGuiFrameResources* frame = &renderer->frames[vk::context().in_flight_frame_idx];
	upload_draw_data(frame, draw_data);

	VkPipelineColorBlendAttachmentState blend = {};
	blend.blendEnable = VK_TRUE;
	blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend.colorBlendOp = VK_BLEND_OP_ADD;
	blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend.alphaBlendOp = VK_BLEND_OP_ADD;
	blend.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	ImGuiPushConstants pc = {
		.scale = {2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y},
		.translate = {-1.0f - draw_data->DisplayPos.x * (2.0f / draw_data->DisplaySize.x),
					  -1.0f - draw_data->DisplayPos.y * (2.0f / draw_data->DisplaySize.y)},
	};

	rg::add_gfx(
		CSTR("ImGui"),
		{.shaders = {{CSTR("src/shaders/imgui.vert")}, {CSTR("src/shaders/imgui.frag")}},
		 .width = output->extent.width,
		 .height = output->extent.height,
		 .cull_mode = VK_CULL_MODE_NONE,
		 .vertex_buffers = {frame->vertex_buffer},
		 .index_buffer = frame->index_buffer,
		 .vertex_bindings = {{.binding = 0, .stride = sizeof(ImDrawVert), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}},
		 .vertex_attributes =
			 {
				 {.location = 0,
				  .binding = 0,
				  .format = VK_FORMAT_R32G32_SFLOAT,
				  .offset = IM_OFFSETOF(ImDrawVert, pos)},
				 {.location = 1,
				  .binding = 0,
				  .format = VK_FORMAT_R32G32_SFLOAT,
				  .offset = IM_OFFSETOF(ImDrawVert, uv)},
				 {.location = 2,
				  .binding = 0,
				  .format = VK_FORMAT_R8G8B8A8_UNORM,
				  .offset = IM_OFFSETOF(ImDrawVert, col)},
			 },
		 .blend_attachments = {blend},
		 .color_load_ops = {VK_ATTACHMENT_LOAD_OP_LOAD},
		 .index_type = sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32,
		 .depth_test_enable = VK_FALSE,
		 .depth_write_enable = VK_FALSE,
		 .color_outputs = {output},
		 .pass_func = render_draw_data})
		.push_constants(&pc)
		.bind(renderer->font_texture);
}

void destroy(ImGuiRenderer* renderer) {
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->SetTexID(nullptr);
	io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
	io.BackendRendererName = nullptr;
	io.BackendRendererUserData = nullptr;
	for (ImGuiFrameResources& frame : renderer->frames) {
		prm::remove(frame.vertex_buffer);
		prm::remove(frame.index_buffer);
		frame = {};
	}
	prm::remove(renderer->font_texture);
	renderer->font_texture = nullptr;
	Window::imgui_shutdown();
	ImGui::DestroyContext();
}

}  // namespace imgui_renderer
