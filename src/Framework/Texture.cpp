#include "../LumenPCH.h"
#include "Texture.h"
#include "CommandBuffer.h"
#include "DynamicResourceManager.h"
#include "Framework/VulkanContext.h"
#include "VkUtils.h"
#include "VulkanContext.h"
#include "VulkanStructs.h"
#include <gli/gli.hpp>
#include <stb_image/stb_image.h>
#include <vulkan/vulkan_core.h>
#include "PersistentResourceManager.h"

static void cmd_generate_mipmaps2(vk::Texture* texture, const VkImageCreateInfo& info, VkCommandBuffer cmd) {
	VkFormatProperties format_properties;
	vkGetPhysicalDeviceFormatProperties(vk::context().physical_device, info.format, &format_properties);
	LUMEN_ASSERT((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT),
				 "Texture image format doesn't support linear blitting");

	VkImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask = texture->aspect_flags;
	subresource_range.baseArrayLayer = 0;
	subresource_range.layerCount = 1;
	subresource_range.levelCount = 1;
	int mip_width = info.extent.width;
	int mip_height = info.extent.height;
	for (uint32_t i = 1; i < texture->mip_levels; i++) {
		subresource_range.baseMipLevel = i - 1;
		vk::transition_image_layout(cmd, texture->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresource_range, texture->aspect_flags);
		VkImageBlit blit{};
		blit.srcOffsets[0] = {0, 0, 0};
		blit.srcOffsets[1] = {mip_width, mip_height, 1};
		blit.srcSubresource.aspectMask = texture->aspect_flags;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = {0, 0, 0};
		blit.dstOffsets[1] = {mip_width > 1 ? mip_width >> 1 : 1, mip_height > 1 ? mip_height >> 1 : 1, 1};
		blit.dstSubresource.aspectMask = texture->aspect_flags;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
		vkCmdBlitImage(cmd, texture->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->handle,
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		vk::transition_image_layout(cmd, texture->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresource_range, texture->aspect_flags);

		if (mip_width > 1) mip_width >>= 1;
		if (mip_height > 1) mip_height >>= 1;
	}
	subresource_range.baseMipLevel = texture->mip_levels - 1;
	vk::transition_image_layout(cmd, texture->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresource_range, texture->aspect_flags);
}

namespace vk {

VkImageAspectFlags get_aspect_flags(VkFormat format) {
	VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_NONE;
	switch (format) {
		case VK_FORMAT_S8_UINT:
			aspect_flags = VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
			aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			break;
		default:
			aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
			break;
	}
	return aspect_flags;
}
void create_texture(Texture* texture, const TextureDesc& desc) {
	texture->name = desc.name;
	texture->extent = desc.dimensions;
	texture->format = desc.format;
	texture->usage_flags = desc.usage;
	texture->aspect_flags = get_aspect_flags(texture->format);
	texture->mip_levels =
		desc.calc_mips ? calc_mip_levels(VkExtent2D(desc.dimensions.width, desc.dimensions.height)) : desc.num_mips;
	texture->array_layers = desc.array_layers;
	texture->layout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (desc.calc_mips) {
		texture->usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	VkImageCreateInfo image_ci;
	if (!desc.image) {
		image_ci = vk::image(texture->format, texture->usage_flags, texture->extent, desc.image_type,
							 texture->mip_levels, desc.array_layers, desc.sample_count, desc.tiling, texture->layout);
		VmaAllocationCreateInfo alloc_ci = {};
		alloc_ci.usage = VMA_MEMORY_USAGE_AUTO;

		VmaAllocationInfo alloc_info;

		vk::check(vmaCreateImage(vk::context().allocator, &image_ci, &alloc_ci, &texture->handle, &texture->allocation,
								 &alloc_info));
	} else {
		texture->handle = desc.image;
	}

	if (!texture->name.empty()) {
		vk::DebugMarker::set_resource_name(vk::context().device, (uint64_t)texture->handle, texture->name.data(),
										   VK_OBJECT_TYPE_IMAGE);
	}

	if (desc.sampler) {
		texture->sampler = desc.sampler;
	} else if (texture->usage_flags & VK_IMAGE_USAGE_SAMPLED_BIT) {
		VkSamplerCreateInfo sampler_CI = vk::sampler();
		sampler_CI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_CI.magFilter = desc.sampler_filter;
		sampler_CI.minFilter = desc.sampler_filter;
		sampler_CI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler_CI.addressModeU = desc.sampler_address_mode;
		sampler_CI.addressModeV = desc.sampler_address_mode;
		sampler_CI.addressModeW = desc.sampler_address_mode;
		sampler_CI.mipLodBias = 0.0f;
		sampler_CI.compareOp = VK_COMPARE_OP_NEVER;
		sampler_CI.minLod = 0.0f;
		sampler_CI.maxLod = (float)texture->mip_levels;
		sampler_CI.anisotropyEnable = vk::context().supported_features.samplerAnisotropy;
		sampler_CI.maxAnisotropy = vk::context().supported_features.samplerAnisotropy
									   ? vk::context().device_properties.limits.maxSamplerAnisotropy
									   : 1.0f;
		sampler_CI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		texture->sampler = prm::get_sampler(sampler_CI);
	}

	VkImageSubresourceRange subresource_range;
	subresource_range.aspectMask = texture->aspect_flags;
	subresource_range.baseArrayLayer = 0;
	subresource_range.layerCount = 1;
	subresource_range.baseMipLevel = 0;
	subresource_range.levelCount = texture->mip_levels;
	if (desc.data.data) {
		Buffer* staging_buffer = drm::get({.name = "Scratch Buffer",
										   .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
										   .memory_type = BufferType::STAGING,
										   .size = desc.data.size,
										   .data = desc.data.data});
		// Copy from staging buffer to image
		lumen::CommandBuffer copy_cmd(true);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = texture->aspect_flags;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageExtent.width = texture->extent.width;
		region.imageExtent.height = texture->extent.height;
		region.imageExtent.depth = 1;
		transition_image_layout(copy_cmd.handle, texture->handle, texture->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								subresource_range, texture->aspect_flags);
		vkCmdCopyBufferToImage(copy_cmd.handle, staging_buffer->handle, texture->handle,
							   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		if (desc.calc_mips) {
			LUMEN_ASSERT(!desc.image, "Cannot generate mips for an image that was not created by the texture");
			cmd_generate_mipmaps2(texture, image_ci, copy_cmd.handle);
		} else {
			transition_image_layout(copy_cmd.handle, texture->handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresource_range, texture->aspect_flags);
		}
		texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		copy_cmd.submit();
		drm::destroy(staging_buffer);
	}

	if (desc.initial_layout != VK_IMAGE_LAYOUT_UNDEFINED) {
		lumen::CommandBuffer cmd(true);
		transition_image_layout(cmd.handle, texture->handle, texture->layout, desc.initial_layout, subresource_range,
								texture->aspect_flags);
		cmd.submit();
		texture->layout = desc.initial_layout;
	}

	VkImageViewCreateInfo image_view_ci = vk::image_view(texture->handle, texture->format, texture->aspect_flags,
														 texture->mip_levels, texture->array_layers);
	vk::check(vkCreateImageView(vk::context().device, &image_view_ci, nullptr, &texture->view),
			  "Failed to create image view!");
}
void destroy_texture(Texture* texture) {
	if (texture->allocation) {
		vmaDestroyImage(vk::context().allocator, texture->handle, texture->allocation);
	}
	vkDestroyImageView(vk::context().device, texture->view, nullptr);
}

VkDescriptorImageInfo get_texture_descriptor(const Texture* tex, VkSampler sampler, VkImageLayout layout) {
	VkDescriptorImageInfo desc_info;
	desc_info.sampler = sampler;
	desc_info.imageView = tex->view;
	desc_info.imageLayout = layout;
	return desc_info;
}
VkDescriptorImageInfo get_texture_descriptor(const Texture* tex, VkImageLayout layout) {
	// LUMEN_ASSERT(!present && sampler, "Sampler not found");
	VkDescriptorImageInfo desc_info;
	desc_info.sampler = tex->sampler;
	desc_info.imageView = tex->view;
	desc_info.imageLayout = layout;
	return desc_info;
}
VkDescriptorImageInfo get_texture_descriptor(const Texture* tex, VkSampler sampler) {
	VkDescriptorImageInfo desc_info;
	desc_info.sampler = sampler;
	desc_info.imageView = tex->view;
	desc_info.imageLayout = tex->layout;
	return desc_info;
}
VkDescriptorImageInfo get_texture_descriptor(const Texture* tex) {
	// LUMEN_ASSERT(!present && sampler, "Sampler not found");
	VkDescriptorImageInfo desc_info;
	desc_info.sampler = tex->sampler;
	desc_info.imageView = tex->view;
	desc_info.imageLayout = tex->layout;
	return desc_info;
}

void force_transition_texture(Texture* tex, VkCommandBuffer cmd, VkImageLayout old_layout, VkImageLayout new_layout) {
	VkImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource_range.baseArrayLayer = 0;
	subresource_range.layerCount = 1;
	subresource_range.baseMipLevel = 0;
	subresource_range.levelCount = tex->mip_levels;
	vk::transition_image_layout(cmd, tex->handle, old_layout, new_layout, subresource_range, tex->aspect_flags);
}

void transition_texture(Texture* tex, VkCommandBuffer cmd, VkImageLayout new_layout) {
	if (tex->layout == new_layout) {
		return;
	}
	VkImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource_range.baseArrayLayer = 0;
	subresource_range.layerCount = 1;
	subresource_range.baseMipLevel = 0;
	subresource_range.levelCount = tex->mip_levels;
	vk::transition_image_layout(cmd, tex->handle, tex->layout, new_layout, subresource_range, tex->aspect_flags);
	tex->layout = new_layout;
}

}  // namespace vk
