#pragma once
#include "VulkanStructs.h"
#include "Framework/Base/SmallArray.h"

namespace vk {

inline constexpr i32 MAX_FRAMES_IN_FLIGHT = 1;
inline constexpr u32 MAX_SWAPCHAIN_IMAGES = 8;
inline constexpr u32 MAX_QUEUES = 16;
inline constexpr u32 MAX_COMMAND_POOLS = 64;

struct VulkanContext {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;
    
    // Swapchain related stuff
    VkSwapchainKHR swapchain;
    
    lm::SmallArray<VkCommandPool, MAX_COMMAND_POOLS> cmd_pools;
    lm::SmallArray<VkQueue, MAX_QUEUES> queues;
    
    QueueFamilyIndices queue_indices;
    
    // Command buffers usually match the number of swapchain images
    lm::SmallArray<VkCommandBuffer, MAX_SWAPCHAIN_IMAGES> command_buffers;
    
    VkPhysicalDeviceFeatures supported_features;
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
    
    VmaAllocator allocator;
    VkQueryPool query_pool_timestamps[3];
    u64 in_flight_frame_idx = 0;
    bool vsync_enabled = false;
};

VulkanContext& context();

};  // namespace vk