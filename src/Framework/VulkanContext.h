
#pragma once
#include "VulkanStructs.h"
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vma/vk_mem_alloc.h>

namespace vk {
inline constexpr int MAX_FRAMES_IN_FLIGHT = 3;

struct VulkanContext {
	VkInstance instance;
	VkDebugUtilsMessengerEXT debug_messenger;
	VkSurfaceKHR surface;
	VkPhysicalDevice physical_device;
	VkDevice device;
	// Swapchain related stuff
	VkSwapchainKHR swapchain;
	std::vector<VkCommandPool> cmd_pools;
	std::vector<VkQueue> queues;
	QueueFamilyIndices queue_indices;
	std::vector<VkCommandBuffer> command_buffers;
	VkPhysicalDeviceFeatures supported_features;
	VkPhysicalDeviceProperties device_properties;
	VkPhysicalDeviceMemoryProperties memory_properties;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
	VmaAllocator allocator;
	VkQueryPool query_pool_timestamps[3];
	size_t in_flight_frame_idx = 0;
	bool vsync_enabled = false;
};

VulkanContext& context();

};	// namespace vk