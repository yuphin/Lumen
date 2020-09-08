#pragma once
#include "Buffer.h"
#include "Lumen.h"
#include "lmhpch.h"


void create_buffer(VkBufferUsageFlags usage,
	VkMemoryPropertyFlags mem_property_flags,
	VkSharingMode sharing_mode,
	Buffer& buffer,
	VkDeviceSize size,
	void* data = nullptr);


uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags props);


void transition_image_layout(VkCommandBuffer copy_cmd, VkImage image, VkImageLayout old_layout,
							 VkImageLayout new_layout, VkImageSubresourceRange subresource_range);

// In case the user wants to specify source and destination stages
void transition_image_layout(VkCommandBuffer copy_cmd, VkImage image, VkImageLayout old_layout, 
						     VkImageLayout new_layout, VkPipelineStageFlags source_stage,
							 VkPipelineStageFlags destination_stage, VkImageSubresourceRange subresource_range);

VkImageView create_image_view(const VkImage& img, VkFormat format);

