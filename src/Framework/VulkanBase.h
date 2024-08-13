#pragma once
#include "LumenPCH.h"
#include <volk/volk.h>
#include "RenderGraph.h"

namespace vk {

void init_imgui();
void init(bool validation_layers);
void destroy_imgui();
void cleanup_swapchain();
void recreate_swap_chain();
void add_device_extension(const char* name);

std::vector<Texture*>& swapchain_images();
uint32_t prepare_frame();
VkResult submit_frame(uint32_t image_idx);

lumen::RenderGraph* render_graph();

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities = {};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> present_modes;
};
bool check_validation_layer_support();
std::vector<const char*> get_req_extensions();
QueueFamilyIndices find_queue_families(VkPhysicalDevice device);
VkResult vkExt_create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
									  const VkAllocationCallbacks* pAllocator,
									  VkDebugUtilsMessengerEXT* pDebugMessenger);

void vkExt_destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger,
								   const VkAllocationCallbacks* pAllocator);

SwapChainSupportDetails query_swapchain_support(VkPhysicalDevice device);

void cleanup_app_data();
void cleanup();
};	// namespace vk
