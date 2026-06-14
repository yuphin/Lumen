#pragma once
#include "RenderGraph.h"

namespace vk {

void init(bool validation_layers);
void add_device_extension(const char* name);
void recreate_swap_chain();

lm::SmallArray<Texture*, MAX_SWAPCHAIN_IMAGES>& swapchain_images();

u32 prepare_frame();
VkResult submit_frame(u32 image_idx);
void cleanup_app_data();
void cleanup();
};	// namespace vk
