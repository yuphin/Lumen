#pragma once
#include "../LumenPCH.h"
#include "GltfScene.hpp"
#include "LumenScene.h"
#include "Buffer.h"
#include "Pipeline.h"
uint32_t find_memory_type(VkPhysicalDevice* physical_device, uint32_t type_filter, VkMemoryPropertyFlags props);

void transition_image_layout(VkCommandBuffer copy_cmd, VkImage image, VkImageLayout old_layout,
							 VkImageLayout new_layout, VkImageSubresourceRange subresource_range,
							 VkImageAspectFlags aspect_flags);

VkImageView create_image_view(VkDevice device, const VkImage& img, VkFormat format,
							  VkImageAspectFlags flags = VK_IMAGE_ASPECT_COLOR_BIT);

BlasInput to_vk_geometry(GltfPrimMesh& prim, VkDeviceAddress vertex_address, VkDeviceAddress index_address);
BlasInput to_vk_geometry(LumenPrimMesh& prim, VkDeviceAddress vertex_address, VkDeviceAddress index_address);

inline bool has_extension(std::string_view filename, std::string_view ext) { return filename.ends_with(ext); }

inline VkTransformMatrixKHR to_vk_matrix(const glm::mat4& mat) {
	glm::mat4 temp = glm::transpose(mat);
	VkTransformMatrixKHR out_matrix;
	memcpy(&out_matrix, &temp, sizeof(VkTransformMatrixKHR));
	return out_matrix;
}

VkPipelineStageFlags get_pipeline_stage(PassType pass_type, VkAccessFlags access_flags);
VkImageLayout get_image_layout(VkDescriptorType type);

template <class T>
constexpr T align_up(T x, size_t a) noexcept {
	return T((x + (T(a) - 1)) & ~T(a - 1));
}

inline VkBufferMemoryBarrier buffer_barrier(VkBuffer buffer, VkAccessFlags src_accesss, VkAccessFlags dst_access) {
	VkBufferMemoryBarrier result = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
	result.srcAccessMask = src_accesss;
	result.dstAccessMask = dst_access;
	result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	result.buffer = buffer;
	result.offset = 0;
	result.size = VK_WHOLE_SIZE;
	return result;
}

inline VkBufferMemoryBarrier2 buffer_barrier2(VkBuffer buffer, VkAccessFlags src_access, VkAccessFlags dst_access,
											  VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage) {
	VkBufferMemoryBarrier2 result = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
	result.srcAccessMask = src_access;
	result.dstAccessMask = dst_access;
	result.srcStageMask = src_stage;
	result.dstStageMask = dst_stage;
	result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	result.buffer = buffer;
	result.offset = 0;
	result.size = VK_WHOLE_SIZE;
	return result;
}

inline VkImageMemoryBarrier image_barrier(VkImage image, VkAccessFlags src_accesss, VkAccessFlags dst_access,
										  VkImageLayout old_layout, VkImageLayout new_layout,
										  VkImageAspectFlags aspect_mask) {
	VkImageMemoryBarrier result = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};

	result.srcAccessMask = src_accesss;
	result.dstAccessMask = dst_access;
	result.oldLayout = old_layout;
	result.newLayout = new_layout;
	result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	result.image = image;
	result.subresourceRange.aspectMask = aspect_mask;
	result.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	return result;
}

inline VkImageMemoryBarrier2 image_barrier2(VkImage image, VkAccessFlags src_accesss, VkAccessFlags dst_access,
											VkImageLayout old_layout, VkImageLayout new_layout,
											VkImageAspectFlags aspect_mask, VkPipelineStageFlags src_stage,
											VkPipelineStageFlags dst_stage) {
	VkImageMemoryBarrier2 result = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};

	result.srcAccessMask = src_accesss;
	result.dstAccessMask = dst_access;
	result.oldLayout = old_layout;
	result.newLayout = new_layout;
	result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	result.image = image;
	result.subresourceRange.aspectMask = aspect_mask;
	result.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	result.srcStageMask = src_stage;
	result.dstStageMask = dst_stage;
	return result;
}

inline VkImageMemoryBarrier2 image_barrier2(VkImage image, VkAccessFlags src_accesss, VkAccessFlags dst_access,
											VkImageLayout old_layout, VkImageLayout new_layout,
											VkImageAspectFlags aspect_mask, VkPipelineStageFlags src_stage,
											VkPipelineStageFlags dst_stage, uint32_t queue_idx) {
	VkImageMemoryBarrier2 result = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};

	result.srcAccessMask = src_accesss;
	result.dstAccessMask = dst_access;
	result.oldLayout = old_layout;
	result.newLayout = new_layout;
	result.srcQueueFamilyIndex = queue_idx;
	result.dstQueueFamilyIndex = queue_idx;
	result.image = image;
	result.subresourceRange.aspectMask = aspect_mask;
	result.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	result.srcStageMask = src_stage;
	result.dstStageMask = dst_stage;
	return result;
}

inline VkDeviceAddress get_device_address(VkDevice device, VkBuffer handle) {
	VkBufferDeviceAddressInfo info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
	info.buffer = handle;
	return vkGetBufferDeviceAddress(device, &info);
}

inline VkDeviceSize get_memory_usage(VkPhysicalDevice physical_device) {
	VkPhysicalDeviceMemoryProperties2 props = {};
	props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
	VkPhysicalDeviceMemoryBudgetPropertiesEXT budget_props = {};
	budget_props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
	props.pNext = &budget_props;
	vkGetPhysicalDeviceMemoryProperties2(physical_device, &props);
	return budget_props.heapUsage[0];
}

inline uint32_t calc_mip_levels(VkExtent2D extent) {
	return static_cast<uint32_t>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;
}

VkImageCreateInfo make_img2d_ci(const VkExtent2D& size, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
								VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT, bool mipmaps = false);

void dispatch_compute(const Pipeline& pipeline, VkCommandBuffer cmdbuf, int wg_x, int wg_y, int width, int height);

uint32_t get_bindings(const std::vector<Shader>& shaders, VkDescriptorType* descriptor_types);

VkImageLayout get_target_img_layout(const Texture2D& tex, VkAccessFlags access_flags);

namespace DebugMarker {
inline void set_resource_name(VkDevice device, uint64_t obj, const char* name, VkObjectType type) {
#if _DEBUG
	VkDebugUtilsObjectNameInfoEXT debug_utils_name{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, nullptr, type,
												   obj, name};
	vkSetDebugUtilsObjectNameEXT(device, &debug_utils_name);
#endif
}
inline void begin_region(VkDevice device, VkCommandBuffer cmd, const char* name, glm::vec4 color) {
	auto pfnCmdDebugMarkerBegin =
		reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT"));
	if (pfnCmdDebugMarkerBegin) {
		VkDebugMarkerMarkerInfoEXT info = {};
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		memcpy(info.color, &color[0], sizeof(float) * 4);
		info.pMarkerName = name;
		pfnCmdDebugMarkerBegin(cmd, &info);
	}
}

inline void end_region(VkDevice device, VkCommandBuffer cmd) {
	auto pfnCmdDebugMarkerEnd =
		reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT"));
	if (pfnCmdDebugMarkerEnd) {
		pfnCmdDebugMarkerEnd(cmd);
	}
}
inline void insert(VkDevice device, VkCommandBuffer cmd, const char* name, glm::vec4 color) {
	auto pfnCmdDebugMarkerInsert =
		reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT"));
	if (pfnCmdDebugMarkerInsert) {
		VkDebugMarkerMarkerInfoEXT info = {};
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		memcpy(info.color, &color[0], sizeof(float) * 4);
		info.pMarkerName = name;
		pfnCmdDebugMarkerInsert(cmd, &info);
	}
}
}  // namespace DebugMarker
