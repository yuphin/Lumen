#pragma once
#include "VulkanContext.h"

namespace vk {
struct Buffer;
struct Texture;
}

struct ImGuiFrameResources {
	vk::Buffer* vertex_buffer = nullptr;
	vk::Buffer* index_buffer = nullptr;
	u64 vertex_capacity = 0;
	u64 index_capacity = 0;
};

struct ImGuiRenderer {
	vk::Texture* font_texture = nullptr;
	ImGuiFrameResources frames[vk::MAX_FRAMES_IN_FLIGHT] = {};
};

namespace imgui_renderer {

void init(ImGuiRenderer* renderer);
void new_frame(ImGuiRenderer* renderer);
void add_pass(ImGuiRenderer* renderer, vk::Texture* output);
void destroy(ImGuiRenderer* renderer);

}  // namespace imgui_renderer
