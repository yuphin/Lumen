#include "lmhpch.h"
#include "Utils.h"


uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags props) {
	const auto& physical_device = Lumen::get()->get_physical_device();
	VkPhysicalDeviceMemoryProperties mem_props;
	vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
	for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
		if ((type_filter & (1 << i)) &&
			(mem_props.memoryTypes[i].propertyFlags & props) == props) {
			return i;
		}
	}
	LUMEN_ASSERT(true, "Failed to find suitable memory type!");
	return static_cast<uint32_t>(-1);
}
void transition_image_layout(VkCommandBuffer copy_cmd, VkImage image, VkImageLayout old_layout, 
							VkImageLayout new_layout,
							VkPipelineStageFlags source_stage,
							VkPipelineStageFlags destination_stage, 
							VkImageSubresourceRange subresource_range) {

	// Create an image barrier object
	VkImageMemoryBarrier image_memory_barrier = vks::image_memory_barrier();
	image_memory_barrier.oldLayout = old_layout;
	image_memory_barrier.newLayout = new_layout;
	image_memory_barrier.image = image;
	image_memory_barrier.subresourceRange = subresource_range;
	// Source layouts (old)
	// Source access mask controls actions that have to be finished on the old layout
	// before it will be transitioned to the new layout
	switch (old_layout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		image_memory_barrier.srcAccessMask = 0;
		break;

	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		// Used for linear images
		image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		image_memory_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	default:
		// Other source layouts aren't handled (yet)
		break;
	}

	// Target layouts (new)
	// Destination access mask controls the dependency for the new image layout
	switch (new_layout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		image_memory_barrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		if (image_memory_barrier.srcAccessMask == 0)
		{
			image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		break;
	default:
		// Other source layouts aren't handled (yet)
		break;
	}

	// Put barrier inside setup command buffer
	vkCmdPipelineBarrier(
		copy_cmd,
		source_stage,
		destination_stage,
		0,
		0, nullptr,
		0, nullptr,
		1, &image_memory_barrier);


}

VkImageView create_image_view(const VkImage& img, VkFormat format) {
	VkImageView image_view;
	VkImageViewCreateInfo image_view_CI = vks::image_view_CI();
	image_view_CI.image = img;
	image_view_CI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_CI.format = format;
	image_view_CI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_CI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_CI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_CI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_CI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_view_CI.subresourceRange.baseMipLevel = 0;
	image_view_CI.subresourceRange.levelCount = 1;
	image_view_CI.subresourceRange.baseArrayLayer = 0;
	image_view_CI.subresourceRange.layerCount = 1;
	const auto& device = Lumen::get()->get_device();
	vks::check(
		vkCreateImageView(device, &image_view_CI, nullptr, &image_view),
		"Failed to create image view!"
	);
	return image_view;
}


void transition_image_layout(VkCommandBuffer copy_cmd, VkImage image, VkImageLayout old_layout, 
						     VkImageLayout new_layout, VkImageSubresourceRange subresource_range) {

	// Create an image barrier object
	VkImageMemoryBarrier image_memory_barrier = vks::image_memory_barrier();
	image_memory_barrier.oldLayout = old_layout;
	image_memory_barrier.newLayout = new_layout;
	image_memory_barrier.image = image;
	image_memory_barrier.subresourceRange = subresource_range;

	// Source layouts (old)
	// Source access mask controls actions that have to be finished on the old layout
	// before it will be transitioned to the new layout

	VkPipelineStageFlags source_stage;
	VkPipelineStageFlags destination_stage;
	switch (old_layout)
	{
	case VK_IMAGE_LAYOUT_UNDEFINED:
		image_memory_barrier.srcAccessMask = 0;
		source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		break;

	case VK_IMAGE_LAYOUT_PREINITIALIZED:
		// Used for linear images
		image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		source_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		image_memory_barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		source_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		break;

	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		source_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	default:
		// Other source layouts aren't handled (yet)
		break;
	}

	// Target layouts (new)
	// Destination access mask controls the dependency for the new image layout
	switch (new_layout)
	{
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		destination_stage = VK_ACCESS_TRANSFER_WRITE_BIT;
		break;

	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		destination_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		break;

	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
		image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;

	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		image_memory_barrier.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		break;

	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		if (image_memory_barrier.srcAccessMask == 0)
		{
			image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
		}
		image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		break;
	default:
		// Other source layouts aren't handled (yet)
		break;
	}

	// Put barrier inside setup command buffer
	vkCmdPipelineBarrier(
		copy_cmd,
		source_stage,
		destination_stage,
		0,
		0, nullptr,
		0, nullptr,
		1, &image_memory_barrier);


}
void create_buffer(VkBufferUsageFlags usage,
	VkMemoryPropertyFlags mem_property_flags,
	VkSharingMode sharing_mode,
	Buffer& buffer,
	VkDeviceSize size,
	void* data) {
	const auto& device = Lumen::get()->get_device();
	buffer.device = device;

	// Create the buffer handle
	VkBufferCreateInfo buffer_CI = vks::buffer_create_info(
		usage,
		size,
		sharing_mode
	);


	vks::check(
		vkCreateBuffer(device, &buffer_CI, nullptr, &buffer.handle),
		"Failed to create vertex buffer!"
	);

	// Create the memory backing up the buffer handle
	VkMemoryRequirements mem_reqs;
	VkMemoryAllocateInfo mem_alloc_info = vks::memory_allocate_info();
	vkGetBufferMemoryRequirements(device, buffer.handle, &mem_reqs);

	mem_alloc_info.allocationSize = mem_reqs.size;
	// Find a memory type index that fits the properties of the buffer
	mem_alloc_info.memoryTypeIndex = find_memory_type(mem_reqs.memoryTypeBits, mem_property_flags);
	vks::check(
		vkAllocateMemory(device, &mem_alloc_info, nullptr, &buffer.buffer_memory),
		"Failed to allocate vertex buffer memory!"
	);

	buffer.alignment = mem_reqs.alignment;
	buffer.size = size;
	buffer.usage_flags = usage;
	buffer.mem_property_flags = mem_property_flags;

	// If a pointer to the buffer data has been passed, map the buffer and copy over the data
	if (data != nullptr) {
		buffer.map_memory();
		memcpy(buffer.data, data, size);
		if ((mem_property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
			buffer.flush();
		}
		buffer.unmap();
	}

	// Initialize a default descriptor that covers the whole buffer size
	buffer.prepare_descriptor();
	buffer.bind();
}
