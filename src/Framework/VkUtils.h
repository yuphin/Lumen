#pragma once
namespace vk {
static constexpr u64 MAX_SHADERS_PER_PASS = 8;
static constexpr u64 MAX_BINDINGS = 32;
static constexpr u64 MAX_COLOR_ATTACHMENTS = 8;
static constexpr u64 MAX_DYNAMIC_STATES = 16;
static constexpr u64 MAX_VERTEX_BINDINGS = 16;
static constexpr u64 MAX_VERTEX_ATTRIBUTES = 16;
static constexpr u64 MAX_PUSH_CONSTANT_RANGES = 8;

void transition_image_layout(VkCommandBuffer copy_cmd, VkImage image, VkImageLayout old_layout,
							 VkImageLayout new_layout, VkImageSubresourceRange subresource_range,
							 VkImageAspectFlags aspect_flags);


inline VkTransformMatrixKHR to_vk_matrix(const lm::mat4& mat) {
	lm::mat4 temp = lm::transpose(mat);
	VkTransformMatrixKHR out_matrix;
	memcpy(&out_matrix, &temp, sizeof(VkTransformMatrixKHR));
	return out_matrix;
}

VkImageLayout image_layout_from_descriptor_type(VkDescriptorType type);

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
											VkPipelineStageFlags dst_stage, u32 queue_idx) {
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

inline u32 calc_mip_levels(VkExtent2D extent) {
	return static_cast<u32>(lm::log2(lm::max(extent.width, extent.height))) + 1;
}

inline void set_resource_name(VkDevice device, u64 obj, const char* name, VkObjectType type) {
#if USE_VALIDATION_LAYERS
	VkDebugUtilsObjectNameInfoEXT debug_utils_name{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, nullptr, type,
												   obj, name};
	if (vkSetDebugUtilsObjectNameEXT) {
		vkSetDebugUtilsObjectNameEXT(device, &debug_utils_name);
	}
#endif
}
inline void begin_region(VkDevice device, VkCommandBuffer cmd, const char* name, lm::vec4 color) {
	PFN_vkCmdDebugMarkerBeginEXT pfnCmdDebugMarkerBegin =
		reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT"));
	if (pfnCmdDebugMarkerBegin) {
		VkDebugMarkerMarkerInfoEXT info = {};
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		memcpy(info.color, &color[0], sizeof(f32) * 4);
		info.pMarkerName = name;
		pfnCmdDebugMarkerBegin(cmd, &info);
	}
}

inline void end_region(VkDevice device, VkCommandBuffer cmd) {
	PFN_vkCmdDebugMarkerEndEXT pfnCmdDebugMarkerEnd =
		reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT"));
	if (pfnCmdDebugMarkerEnd) {
		pfnCmdDebugMarkerEnd(cmd);
	}
}
inline void insert(VkDevice device, VkCommandBuffer cmd, const char* name, lm::vec4 color) {
	PFN_vkCmdDebugMarkerInsertEXT pfnCmdDebugMarkerInsert =
		reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT"));
	if (pfnCmdDebugMarkerInsert) {
		VkDebugMarkerMarkerInfoEXT info = {};
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		memcpy(info.color, &color[0], sizeof(f32) * 4);
		info.pMarkerName = name;
		pfnCmdDebugMarkerInsert(cmd, &info);
	}
}

}  // namespace vk
