#include "LumenPCH.h"
#include "Utils.h"
#include "CommandBuffer.h"
#include "Framework/ThreadPool.h"

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
		case VK_IMAGE_LAYOUT_UNDEFINED:
			source_stage = VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
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
	VkImageViewCreateInfo image_view_CI = vk::image_view_CI();
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
	vk::check(vkCreateImageView(device, &image_view_CI, nullptr, &image_view), "Failed to create image view!");
	return image_view;
}

BlasInput to_vk_geometry(GltfPrimMesh& prim, VkDeviceAddress vertexAddress, VkDeviceAddress indexAddress) {
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

BlasInput to_vk_geometry(LumenPrimMesh& prim, VkDeviceAddress vertex_address, VkDeviceAddress index_address) {
	uint32_t maxPrimitiveCount = prim.idx_count / 3;

	// Describe buffer as array of VertexObj.
	VkAccelerationStructureGeometryTrianglesDataKHR triangles{
		VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;  // vec3 vertex position data.
	triangles.vertexData.deviceAddress = vertex_address;
	triangles.vertexStride = sizeof(glm::vec3);
	// Describe index data (32-bit unsigned int)
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = index_address;
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

VkRenderPass create_render_pass(VkDevice device, const std::vector<VkFormat>& color_attachment_formats,
								VkFormat depth_attachment_format, uint32_t subpass_count /*= 1*/,
								bool clear_color /*= true*/, bool clear_depth /*= true*/,
								VkImageLayout initial_layout /*= VK_IMAGE_LAYOUT_UNDEFINED */,
								VkImageLayout final_layout /*= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR*/) {
	std::vector<VkAttachmentDescription> all_attachments;
	std::vector<VkAttachmentReference> color_attachment_refs;

	bool has_depth = (depth_attachment_format != VK_FORMAT_UNDEFINED);

	for (const auto& format : color_attachment_formats) {
		VkAttachmentDescription color_attachment = {};
		color_attachment.format = format;
		color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment.loadOp = clear_color
									  ? VK_ATTACHMENT_LOAD_OP_CLEAR
									  : ((initial_layout == VK_IMAGE_LAYOUT_UNDEFINED) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE
																					   : VK_ATTACHMENT_LOAD_OP_LOAD);
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment.initialLayout = initial_layout;
		color_attachment.finalLayout = final_layout;

		VkAttachmentReference color_attachment_ref = {};
		color_attachment_ref.attachment = static_cast<uint32_t>(all_attachments.size());
		color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		all_attachments.push_back(color_attachment);
		color_attachment_refs.push_back(color_attachment_ref);
	}

	VkAttachmentReference depth_attachment_ref = {};
	if (has_depth) {
		VkAttachmentDescription depth_attachment = {};
		depth_attachment.format = depth_attachment_format;
		depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_attachment.loadOp = clear_depth ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;

		depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		depth_attachment_ref.attachment = static_cast<uint32_t>(all_attachments.size());
		depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		all_attachments.push_back(depth_attachment);
	}
	std::vector<VkSubpassDescription> subpasses;
	std::vector<VkSubpassDependency> subpass_dependencies;
	for (uint32_t i = 0; i < subpass_count; i++) {
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = static_cast<uint32_t>(color_attachment_refs.size());
		subpass.pColorAttachments = color_attachment_refs.data();
		subpass.pDepthStencilAttachment = has_depth ? &depth_attachment_ref : VK_NULL_HANDLE;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = i == 0 ? (VK_SUBPASS_EXTERNAL) : (i - 1);
		dependency.dstSubpass = i;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		subpasses.push_back(subpass);
		subpass_dependencies.push_back(dependency);
	}

	VkRenderPassCreateInfo rpi{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	rpi.attachmentCount = static_cast<uint32_t>(all_attachments.size());
	rpi.pAttachments = all_attachments.data();
	rpi.subpassCount = static_cast<uint32_t>(subpasses.size());
	rpi.pSubpasses = subpasses.data();
	rpi.dependencyCount = static_cast<uint32_t>(subpass_dependencies.size());
	rpi.pDependencies = subpass_dependencies.data();
	VkRenderPass rp;
	vk::check(vkCreateRenderPass(device, &rpi, nullptr, &rp));
	return rp;
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

void dispatch_compute(const Pipeline& pipeline, VkCommandBuffer cmdbuf, int wg_x, int wg_y, int width, int height) {
	vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.handle);
	const auto num_wg_x = (uint32_t)ceil(width / float(wg_x));
	const auto num_wg_y = (uint32_t)ceil(height / float(wg_y));
	vkCmdDispatch(cmdbuf, num_wg_x, num_wg_y, 1);
}

glm::ivec2 get_num_wgs(int width, int height, int wg_x_size, int wg_y_size) {
	return glm::ivec2{ceil(width / float(wg_x_size)), ceil(height / float(wg_y_size))};
}

uint32_t get_bindings(const std::vector<Shader>& shaders, VkDescriptorType* descriptor_types) {
	uint32_t binding_mask = 0;
	for (const auto& shader : shaders) {
		for (uint32_t i = 0; i < 32; ++i) {
			if (shader.binding_mask & (1 << i)) {
				if (binding_mask & (1 << i)) {
					LUMEN_ASSERT(descriptor_types[i] == shader.descriptor_types[i],
								 "Binding mask mismatch on shader {}", shader.filename.c_str());
				} else {
					descriptor_types[i] = shader.descriptor_types[i];
					binding_mask |= 1 << i;
				}
			}
		}
	}
	return binding_mask;
}

VkImageLayout get_target_img_layout(const Texture2D& tex, VkAccessFlags access_flags) {
	if ((tex.usage_flags & VK_IMAGE_USAGE_SAMPLED_BIT) && access_flags == VK_ACCESS_SHADER_READ_BIT) {
		return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	}
	return VK_IMAGE_LAYOUT_GENERAL;
}
