#pragma once
#include "Buffer.h"
#include "LumenPCH.h"

uint32_t find_memory_type(VkPhysicalDevice* physical_device, uint32_t type_filter, VkMemoryPropertyFlags props);


void transition_image_layout(VkCommandBuffer copy_cmd, VkImage image, VkImageLayout old_layout,
							 VkImageLayout new_layout, VkImageSubresourceRange subresource_range);

// In case the user wants to specify source and destination stages
void transition_image_layout(VkCommandBuffer copy_cmd, VkImage image, VkImageLayout old_layout,
							 VkImageLayout new_layout, VkPipelineStageFlags source_stage,
							 VkPipelineStageFlags destination_stage, VkImageSubresourceRange subresource_range);

VkImageView create_image_view(VkDevice* device, const VkImage& img, VkFormat format);


inline bool has_extension(std::string_view filename, std::string_view ext) {
	return filename.ends_with(ext);
}
