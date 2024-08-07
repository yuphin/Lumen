#include "../LumenPCH.h"
#include "Texture.h"
#include "CommandBuffer.h"
#include "VkUtils.h"
#include <gli/gli.hpp>
#include <stb_image/stb_image.h>

namespace lumen {

Texture2D::Texture2D(const std::string& name, VkImage image, VkFormat format,
					 VkImageUsageFlags usage_flags, VkImageAspectFlags aspect_flags, VkExtent2D extent, bool present) {
	img = image;
	img_view = vk::create_image_view(vk::context().device, img, format);
	this->present = present;
	this->format = format;
	this->aspect_flags = aspect_flags;
	this->usage_flags = usage_flags;
	this->base_extent = VkExtent3D{extent.width, extent.height, 1};
	if (!name.empty()) {
		this->name = name;
		vk::DebugMarker::set_resource_name(vk::context().device, (uint64_t)img, name.c_str(), VK_OBJECT_TYPE_IMAGE);
	}
}

void Texture2D::transition(VkCommandBuffer cmd, VkImageLayout new_layout) {
	if (layout == new_layout) {
		return;
	}
	VkImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource_range.baseArrayLayer = 0;
	subresource_range.layerCount = 1;
	subresource_range.baseMipLevel = 0;
	subresource_range.levelCount = mip_levels;
	vk::transition_image_layout(cmd, img, layout, new_layout, subresource_range, aspect_flags);
	layout = new_layout;
}

void Texture2D::force_transition(VkCommandBuffer cmd, VkImageLayout old_layout, VkImageLayout new_layout) {
	VkImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource_range.baseArrayLayer = 0;
	subresource_range.layerCount = 1;
	subresource_range.baseMipLevel = 0;
	subresource_range.levelCount = mip_levels;
	vk::transition_image_layout(cmd, img, old_layout, new_layout, subresource_range, aspect_flags);
}

void Texture2D::transition_without_state(VkCommandBuffer cmd, VkImageLayout new_layout) {
	auto old_layout = layout;
	transition(cmd, new_layout);
}

VkDescriptorImageInfo Texture2D::descriptor(VkSampler sampler, VkImageLayout layout) const {
	VkDescriptorImageInfo desc_info;
	desc_info.sampler = sampler;
	desc_info.imageView = img_view;
	desc_info.imageLayout = layout;
	return desc_info;
}

VkDescriptorImageInfo Texture2D::descriptor(VkImageLayout layout) const {
	// LUMEN_ASSERT(!present && sampler, "Sampler not found");
	VkDescriptorImageInfo desc_info;
	desc_info.sampler = sampler;
	desc_info.imageView = img_view;
	desc_info.imageLayout = layout;
	return desc_info;
}

VkDescriptorImageInfo Texture2D::descriptor(VkSampler sampler) const {
	VkDescriptorImageInfo desc_info;
	desc_info.sampler = sampler;
	desc_info.imageView = img_view;
	desc_info.imageLayout = layout;
	return desc_info;
}

VkDescriptorImageInfo Texture2D::descriptor() const {
	// LUMEN_ASSERT(!present && sampler, "Sampler not found");
	VkDescriptorImageInfo desc_info;
	desc_info.sampler = sampler;
	desc_info.imageView = img_view;
	desc_info.imageLayout = layout;
	return desc_info;
}

void Texture2D::load_from_data(void* data, VkDeviceSize size, const VkImageCreateInfo& info,
							   VkSampler a_sampler, VkImageUsageFlags flags, bool generate_mipmaps) {
	aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
	usage_flags = flags;
	Buffer staging_buffer;
	staging_buffer.create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, size, data);

	// Need to do this check pre image creation
	if (generate_mipmaps) {
		usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		mip_levels = info.mipLevels;
	}
	create_image(info);

	// Copy from staging buffer to image
	VkImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask = aspect_flags;
	subresource_range.baseArrayLayer = 0;
	subresource_range.layerCount = 1;
	subresource_range.baseMipLevel = 0;
	subresource_range.levelCount = mip_levels;

	VkBufferImageCopy region{};

	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = aspect_flags;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = info.extent.width;
	region.imageExtent.height = info.extent.height;
	region.imageExtent.depth = 1;
	CommandBuffer copy_cmd(true);
	vk::transition_image_layout(copy_cmd.handle, img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								subresource_range, aspect_flags);

	vkCmdCopyBufferToImage(copy_cmd.handle, staging_buffer.handle, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
						   &region);

	if (generate_mipmaps) {
		cmd_generate_mipmaps(info, copy_cmd.handle);
	} else {
		vk::transition_image_layout(copy_cmd.handle, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresource_range, aspect_flags);
	}
	copy_cmd.submit();
	staging_buffer.destroy();
	img_view = vk::create_image_view(vk::context().device, img, info.format);
	this->sampler = a_sampler;

	layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	base_extent = info.extent;
}

void Texture2D::create_empty_texture(const char* name, const TextureSettings& settings,
									 VkImageLayout img_layout, VkSampler in_sampler /* 0*/,
									 VkImageAspectFlags flags /*=VK_IMAGE_ASPECT_COLOR_BIT*/) {
	auto image_CI = vk::image(settings.format, settings.usage_flags, settings.base_extent);
	image_CI.imageType = settings.image_type;
	image_CI.mipLevels = settings.mip_levels;
	image_CI.arrayLayers = settings.array_layers;
	image_CI.tiling = settings.tiling;
	image_CI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_CI.samples = settings.sample_count;
	image_CI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_image(image_CI);
	img_view = vk::create_image_view(vk::context().device, img, settings.format, flags);
	if (name) {
		vk::DebugMarker::set_resource_name(vk::context().device, (uint64_t)img, name, VK_OBJECT_TYPE_IMAGE);
		this->name = name;
	}
	// Create a default sampler
	if (!in_sampler) {
		sampler_allocated = true;
		VkSamplerCreateInfo sampler_CI = vk::sampler();
		sampler_CI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler_CI.magFilter = VK_FILTER_LINEAR;
		sampler_CI.minFilter = VK_FILTER_LINEAR;
		sampler_CI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler_CI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_CI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_CI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		sampler_CI.mipLodBias = 0.0f;
		sampler_CI.compareOp = VK_COMPARE_OP_NEVER;
		sampler_CI.minLod = 0.0f;
		sampler_CI.maxLod = (float)settings.mip_levels;
		sampler_CI.anisotropyEnable = vk::context().supported_features.samplerAnisotropy;
		sampler_CI.maxAnisotropy =
			vk::context().supported_features.samplerAnisotropy ? vk::context().device_properties.limits.maxSamplerAnisotropy : 1.0f;
		sampler_CI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		vk::check(vkCreateSampler(vk::context().device, &sampler_CI, nullptr, &sampler), "Could not create image sampler");
		if (name) {
			std::string sampler_name = std::string("Sampler: ") + std::string(name);
			vk::DebugMarker::set_resource_name(vk::context().device, (uint64_t)sampler, sampler_name.c_str(),
											   VK_OBJECT_TYPE_SAMPLER);
		}
	} else {
		sampler = in_sampler;
	}

	layout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (img_layout != VK_IMAGE_LAYOUT_UNDEFINED) {
		aspect_flags = flags;
		usage_flags = settings.usage_flags;
		present = settings.present;
		CommandBuffer cmd(true);
		transition(cmd.handle, img_layout);
		cmd.submit();
	}

	base_extent = settings.base_extent;
}

void Texture::create_image(const VkImageCreateInfo& info) {
	vk::check(vkCreateImage(vk::context().device, &info, nullptr, &img), "Failed to create image");

	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements(vk::context().device, img, &mem_req);
	auto alloc_info = vk::memory_allocate_info();
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex =
		vk::find_memory_type(&vk::context().physical_device, mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vk::check(vkAllocateMemory(vk::context().device, &alloc_info, nullptr, &img_mem), "Failed to allocate image memory");
	vkBindImageMemory(vk::context().device, img, img_mem, 0);
	base_extent = info.extent;
}

void Texture::cmd_generate_mipmaps(const VkImageCreateInfo& info, VkCommandBuffer cmd) {
	VkFormatProperties format_properties;
	vkGetPhysicalDeviceFormatProperties(vk::context().physical_device, info.format, &format_properties);
	LUMEN_ASSERT((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT),
				 "Texture image format doesn't support linear blitting");

	VkImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask = aspect_flags;
	subresource_range.baseArrayLayer = 0;
	subresource_range.layerCount = 1;
	subresource_range.levelCount = 1;
	int mip_width = info.extent.width;
	int mip_height = info.extent.height;
	for (uint32_t i = 1; i < mip_levels; i++) {
		subresource_range.baseMipLevel = i - 1;
		vk::transition_image_layout(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresource_range, aspect_flags);
		VkImageBlit blit{};
		blit.srcOffsets[0] = {0, 0, 0};
		blit.srcOffsets[1] = {mip_width, mip_height, 1};
		blit.srcSubresource.aspectMask = aspect_flags;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = {0, 0, 0};
		blit.dstOffsets[1] = {mip_width > 1 ? mip_width >> 1 : 1, mip_height > 1 ? mip_height >> 1 : 1, 1};
		blit.dstSubresource.aspectMask = aspect_flags;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
		vkCmdBlitImage(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
					   &blit, VK_FILTER_LINEAR);

		vk::transition_image_layout(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresource_range, aspect_flags);

		if (mip_width > 1) mip_width >>= 1;
		if (mip_height > 1) mip_height >>= 1;
	}
	subresource_range.baseMipLevel = mip_levels - 1;
	vk::transition_image_layout(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresource_range, aspect_flags);
}

void Texture::destroy() {
	if (sampler_allocated) {
		vkDestroySampler(vk::context().device, sampler, nullptr);
	}
	vkDestroyImageView(vk::context().device, img_view, nullptr);
	if (img_mem) {
		vkDestroyImage(vk::context().device, img, nullptr);
		vkFreeMemory(vk::context().device, img_mem, nullptr);
	}
	img = VK_NULL_HANDLE;
}
}  // namespace lumen
