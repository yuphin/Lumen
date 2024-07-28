
#pragma once
#include "VulkanStructs.h"
namespace VulkanContext {
	extern GLFWwindow* window_ptr;
	extern VkInstance instance;
	extern VkDebugUtilsMessengerEXT debug_messenger;
	extern VkSurfaceKHR surface;
	extern VkPhysicalDevice physical_device;
	extern VkDevice device;
	extern VkSwapchainKHR swapchain;
	extern std::vector<VkCommandPool> cmd_pools;
	extern std::vector<VkQueue> queues;
	extern lumen::QueueFamilyIndices queue_indices;
	extern std::vector<VkCommandBuffer> command_buffers;
	extern VkPhysicalDeviceFeatures supported_features;
	extern VkPhysicalDeviceProperties device_properties;
	extern VkPhysicalDeviceMemoryProperties memory_properties;
	extern VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props;
};