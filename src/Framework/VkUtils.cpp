#include "../LumenPCH.h"
#include "RayTracer/LumenScene.h"
#include "VkUtils.h"

namespace vk {
uint32_t find_memory_type(VkPhysicalDevice* physical_device, uint32_t type_filter, VkMemoryPropertyFlags props) {
	VkPhysicalDeviceMemoryProperties mem_props;
	vkGetPhysicalDeviceMemoryProperties(*physical_device, &mem_props);
	for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
		if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props) {
			return i;
		}
	}
	LUMEN_ASSERT(true, "Failed to find suitable memory type!");
	return static_cast<uint32_t>(-1);
}

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

VkImageView create_image_view(VkDevice device, const VkImage& img, VkFormat format, VkImageAspectFlags flags) {
	VkImageView image_view;
	VkImageViewCreateInfo image_view_CI = vk::image_view();
	image_view_CI.image = img;
	image_view_CI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_CI.format = format;
	image_view_CI.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_CI.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_CI.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_CI.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	image_view_CI.subresourceRange.aspectMask = flags;
	image_view_CI.subresourceRange.baseMipLevel = 0;
	image_view_CI.subresourceRange.levelCount = 1;
	image_view_CI.subresourceRange.baseArrayLayer = 0;
	image_view_CI.subresourceRange.layerCount = 1;
	vk::check(vkCreateImageView(device, &image_view_CI, nullptr, &image_view));
	return image_view;
}

BlasInput to_vk_geometry(LumenPrimMesh& prim, VkDeviceAddress vertexAddress, VkDeviceAddress indexAddress) {
	uint32_t maxPrimitiveCount = prim.idx_count / 3;

	// Describe buffer as array of VertexObj.
	VkAccelerationStructureGeometryTrianglesDataKHR triangles{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;  // vec3 vertex position data.
	triangles.vertexData.deviceAddress = vertexAddress;
	triangles.vertexStride = sizeof(glm::vec3);
	// Describe index data (32-bit unsigned int)
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = indexAddress;
	// Indicate identity transform by setting transformData to null device
	// pointer.
	// triangles.transformData = {};
	triangles.maxVertex = prim.vtx_count;

	// Identify the above data as containing opaque triangles.
	VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
	asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	asGeom.flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;	 // For AnyHit
	asGeom.geometry.triangles = triangles;

	VkAccelerationStructureBuildRangeInfoKHR offset;
	offset.firstVertex = prim.vtx_offset;
	offset.primitiveCount = maxPrimitiveCount;
	offset.primitiveOffset = prim.first_idx * sizeof(uint32_t);
	offset.transformOffset = 0;

	// Our blas is made from only one geometry, but could be made of many
	// geometries
	BlasInput input;
	input.as_geom.emplace_back(asGeom);
	input.as_build_offset_info.emplace_back(offset);

	return input;
}

VkPipelineStageFlags get_pipeline_stage(vk::PassType pass_type, VkAccessFlags access_flags) {
	VkPipelineStageFlags res = 0;
	switch (pass_type) {
		case vk::PassType::Compute:
			res = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;
		case vk::PassType::Graphics:
			res = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
			break;
		case vk::PassType::RT:
			if (access_flags &
				(VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR)) {
				res = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
			} else {
				res = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
			}
			break;
		default:
			res = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;
	}
	if ((access_flags & VK_ACCESS_TRANSFER_READ_BIT) || (access_flags & VK_ACCESS_TRANSFER_WRITE_BIT)) {
		res |= VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	return res;
}

VkImageLayout get_image_layout(VkDescriptorType type) {
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

VkImageCreateInfo make_img2d_ci(const VkExtent2D& size, VkFormat format, VkImageUsageFlags usage, bool mipmaps) {
	VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = format;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.mipLevels = mipmaps ? calc_mip_levels(size) : 1;
	ici.arrayLayers = 1;
	ici.extent.width = size.width;
	ici.extent.height = size.height;
	ici.extent.depth = 1;
	ici.usage = usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	return ici;
}

VkImageLayout get_target_img_layout(const vk::Texture* tex, VkAccessFlags access_flags) {
	if ((tex->usage_flags & VK_IMAGE_USAGE_SAMPLED_BIT) && access_flags == VK_ACCESS_SHADER_READ_BIT) {
		return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	return VK_IMAGE_LAYOUT_GENERAL;
}

}  // namespace vk
