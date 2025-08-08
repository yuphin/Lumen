#include "RayTracer/LumenScene.h"
#include "VkUtils.h"

namespace vk {

void transition_image_layout(VkCommandBuffer copy_cmd, VkImage image, VkImageLayout old_layout,
							 VkImageLayout new_layout, VkPipelineStageFlags source_stage,
							 VkPipelineStageFlags destination_stage, VkImageSubresourceRange subresource_range) {
	// Create an image barrier object
	VkImageMemoryBarrier image_memory_barrier = vk::image_memory_barrier();
	image_memory_barrier.oldLayout = old_layout;
	image_memory_barrier.newLayout = new_layout;
	image_memory_barrier.image = image;
	image_memory_barrier.subresourceRange = subresource_range;
	// Source layouts (old)
	// Source access mask controls actions that have to be finished on the old
	// layout before it will be transitioned to the new layout
	switch (old_layout) {
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
		case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		default:
			// Other source layouts aren't handled (yet)
			break;
	}

	// Target layouts (new)
	// Destination access mask controls the dependency for the new image layout
	switch (new_layout) {
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

		case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			if (image_memory_barrier.srcAccessMask == 0) {
				image_memory_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			}
			image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		default:
			// Other source layouts aren't handled (yet)
			break;
	}

	// Put barrier inside setup command buffer
	vkCmdPipelineBarrier(copy_cmd, source_stage, destination_stage, 0, 0, nullptr, 0, nullptr, 1,
						 &image_memory_barrier);
}

void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,
							 VkImageSubresourceRange subresource_range, VkImageAspectFlags aspect_flags) {
	VkAccessFlags src_access_flags = 0;
	VkAccessFlags dst_access_flags = 0;
	VkPipelineStageFlags source_stage = 0;
	VkPipelineStageFlags destination_stage = 0;
	// Old layout
	switch (old_layout) {
		case VK_IMAGE_LAYOUT_GENERAL:
			src_access_flags = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			source_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			break;
		case VK_IMAGE_LAYOUT_UNDEFINED:
			// TODO: Add custom source stage
			source_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			// Used for linear images
			src_access_flags = VK_ACCESS_HOST_WRITE_BIT;
			source_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			break;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			src_access_flags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			source_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			src_access_flags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			source_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			src_access_flags = VK_ACCESS_TRANSFER_READ_BIT;
			source_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			src_access_flags = VK_ACCESS_TRANSFER_WRITE_BIT;
			source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			src_access_flags = VK_ACCESS_SHADER_READ_BIT;
			source_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			break;
		default:
			source_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;
	}
	// New layout
	switch (new_layout) {
		case VK_IMAGE_LAYOUT_GENERAL:
			dst_access_flags = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			destination_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dst_access_flags = VK_ACCESS_TRANSFER_WRITE_BIT;
			destination_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dst_access_flags = VK_ACCESS_TRANSFER_READ_BIT;
			destination_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			break;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			dst_access_flags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			destination_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dst_access_flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			destination_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			break;
		case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			if (src_access_flags == 0) {
				src_access_flags = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			}
			dst_access_flags = VK_ACCESS_SHADER_READ_BIT;
			destination_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			break;
		default:
			destination_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;
	}

	auto img_barrier = image_barrier2(image, src_access_flags, dst_access_flags, old_layout, new_layout, aspect_flags,
									  source_stage, destination_stage);
	auto dependency_info = vk::dependency_info(1, &img_barrier);
	vkCmdPipelineBarrier2(cmd, &dependency_info);
}

VkImageLayout image_layout_from_descriptor_type(VkDescriptorType type) {
	switch (type) {
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			return VK_IMAGE_LAYOUT_GENERAL;
		default:
			break;
	}
	LUMEN_ERROR("Unhandled image layout case");
	return VK_IMAGE_LAYOUT_GENERAL;
}


}  // namespace vk
