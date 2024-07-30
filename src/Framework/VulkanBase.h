#pragma once
#include "LumenPCH.h"
#include <volk/volk.h>
#include "Buffer.h"
#include "RenderGraph.h"
#include "AccelerationStructure.h"

namespace lumen {

namespace VulkanBase {

struct BuildAccelerationStructure {
	VkAccelerationStructureBuildGeometryInfoKHR build_info{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
	VkAccelerationStructureBuildSizesInfoKHR size_info{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
	const VkAccelerationStructureBuildRangeInfoKHR* range_info;
	AccelKHR as;  // result acceleration structure
	AccelKHR cleanup_as;
};
void init(bool validation_layers);
// Create VKInstance with current extensions
void create_instance();
void setup_debug_messenger();
void create_surface();
void pick_physical_device();
void create_logical_device();
void create_swapchain();
void create_sync_primitives();
void create_command_buffers();
void create_command_pools();
void init_imgui();
void destroy_imgui();
void cleanup_swapchain();
void recreate_swap_chain();
void add_device_extension(const char* name);
void build_blas(std::vector<AccelKHR>& blases, const std::vector<vk::BlasInput>& input, VkBuildAccelerationStructureFlagsKHR flags);
void build_tlas(AccelKHR& tlas, std::vector<VkAccelerationStructureInstanceKHR>& instances,
				VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
				bool update = false);
std::vector<Texture2D>& swapchain_images();
uint32_t prepare_frame();
VkResult submit_frame(uint32_t image_idx);

RenderGraph* render_graph();

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities = {};
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> present_modes;
};
bool check_validation_layer_support();
std::vector<const char*> get_req_extensions();
vk::QueueFamilyIndices find_queue_families(VkPhysicalDevice device);
VkResult vkExt_create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
									  const VkAllocationCallbacks* pAllocator,
									  VkDebugUtilsMessengerEXT* pDebugMessenger);

void vkExt_destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger,
								   const VkAllocationCallbacks* pAllocator);

SwapChainSupportDetails query_swapchain_support(VkPhysicalDevice device);

void cleanup_app_data();
void cleanup();

AccelKHR create_acceleration(VkAccelerationStructureCreateInfoKHR& accel);
void cmd_compact_blas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
					  std::vector<BuildAccelerationStructure>& buildAs, VkQueryPool queryPool);
void cmd_create_blas(VkCommandBuffer cmdBuf, std::vector<uint32_t> indices,
					 std::vector<BuildAccelerationStructure>& buildAs, VkDeviceAddress scratchAddress,
					 VkQueryPool queryPool);

void cmd_create_tlas(VkCommandBuffer cmdBuf,  // Command buffer
					 uint32_t countInstance,  // number of instances
					 Buffer& scratchBuffer,
					 VkDeviceAddress instBufferAddr,			  // Buffer address of instances
					 VkBuildAccelerationStructureFlagsKHR flags,  // Build creation flag
					 bool update								  // Update == animation
);
};	// namespace VulkanBase

}  // namespace lumen
