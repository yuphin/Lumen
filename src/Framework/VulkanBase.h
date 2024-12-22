#pragma once
#include "LumenPCH.h"
#include <volk/volk.h>
#include "RenderGraph.h"

namespace vk {

void init_imgui();
void init(bool validation_layers);
void destroy_imgui();
void add_device_extension(const char* name);
std::vector<Texture*>& swapchain_images();
uint32_t prepare_frame();
VkResult submit_frame(uint32_t image_idx);
lumen::RenderGraph* render_graph();
void cleanup_app_data();
void cleanup();
};	// namespace vk
