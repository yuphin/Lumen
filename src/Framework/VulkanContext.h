
#pragma once
#include "VulkanStructs.h"
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include <vma/vk_mem_alloc.h>

namespace vk {

struct VulkanContext {
	GLFWwindow* window_ptr = nullptr;
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
};

VulkanContext& context();

};	// namespace vk