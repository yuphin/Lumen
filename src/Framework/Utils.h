#pragma once
#include "LumenPCH.h"
#include "gltfscene.hpp"
uint32_t find_memory_type(VkPhysicalDevice* physical_device,
						  uint32_t type_filter, VkMemoryPropertyFlags props);

void transition_image_layout(VkCommandBuffer copy_cmd, VkImage image,
							 VkImageLayout old_layout, VkImageLayout new_layout,
							 VkImageSubresourceRange subresource_range);

// In case the user wants to specify source and destination stages

void transition_image_layout(VkCommandBuffer copy_cmd, VkImage image,
							 VkImageLayout old_layout, VkImageLayout new_layout,
							 VkPipelineStageFlags source_stage,
							 VkPipelineStageFlags destination_stage,
							 VkImageSubresourceRange subresource_range);

inline void transition_image_layout(
	VkCommandBuffer copy_cmd, VkImage image, VkImageLayout old_layout,
	VkImageLayout new_layout,
	VkImageAspectFlags aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT) {
	VkImageSubresourceRange range;
	range.aspectMask = aspect_mask;
	range.levelCount = VK_REMAINING_MIP_LEVELS;
	range.layerCount = VK_REMAINING_ARRAY_LAYERS;
	range.baseMipLevel = 0;
	range.baseArrayLayer = 0;
	transition_image_layout(copy_cmd, image, old_layout, new_layout, range);
}

VkImageView
create_image_view(VkDevice device, const VkImage& img, VkFormat format,
				  VkImageAspectFlags flags = VK_IMAGE_ASPECT_COLOR_BIT);

BlasInput to_vk_geometry(GltfPrimMesh& prim, VkDeviceAddress vertex_address,
						 VkDeviceAddress index_address);

VkRenderPass create_render_pass(
	VkDevice device, const std::vector<VkFormat>& color_attachment_formats,
	VkFormat depth_format, uint32_t subpass_count = 1, bool clear_color = true,
	bool clear_depth = true,
	VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED,
	VkImageLayout final_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

inline bool has_extension(std::string_view filename, std::string_view ext) {
	return filename.ends_with(ext);
}

inline VkTransformMatrixKHR to_vk_matrix(const glm::mat4& mat) {
	glm::mat4 temp = glm::transpose(mat);
	VkTransformMatrixKHR out_matrix;
	memcpy(&out_matrix, &temp, sizeof(VkTransformMatrixKHR));
	return out_matrix;
}

template <class T> constexpr T align_up(T x, size_t a) noexcept {
	return T((x + (T(a) - 1)) & ~T(a - 1));
}

inline VkBufferMemoryBarrier buffer_barrier(VkBuffer buffer,
											VkAccessFlags src_accesss,
											VkAccessFlags dst_access) {
	VkBufferMemoryBarrier result = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
	result.srcAccessMask = src_accesss;
	result.dstAccessMask = dst_access;
	result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	result.buffer = buffer;
	result.offset = 0;
	result.size = VK_WHOLE_SIZE;
	return result;
}

inline VkImageMemoryBarrier image_barrier(VkImage image,
										  VkAccessFlags src_accesss,
										  VkAccessFlags dst_access,
										  VkImageLayout old_layout,
										  VkImageLayout new_layout,
										  VkImageAspectFlags aspect_mask) {
	VkImageMemoryBarrier result = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

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

inline VkDeviceAddress get_device_address(VkDevice device, VkBuffer handle) {
	VkBufferDeviceAddressInfo info = {
		VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = handle;
	return vkGetBufferDeviceAddress(device, &info);
}

inline uint32_t calc_mip_levels(VkExtent2D extent) {
	return static_cast<uint32_t>(
		std::floor(std::log2(std::max(extent.width, extent.height)))) +
		1;
}

VkImageCreateInfo make_img2d_ci(
	const VkExtent2D& size, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
	VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT, bool mipmaps = false);
