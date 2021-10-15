#include "LumenPCH.h"
#include "Texture.h"
#include <stb_image.h>
#include <gli/gli.hpp>
#include "Framework/Utils.h"
#include "Framework/CommandBuffer.h"

Texture2D::Texture2D(VulkanContext* ctx) : Texture(ctx) {}

Texture2D::Texture2D(
	VulkanContext* ctx,
	VkFormat format,
	VkImageTiling tiling,
	VkImageUsageFlags usage_flags,
	uint32_t mip_levels,
	uint32_t array_layers,
	VkSampleCountFlagBits sample_count,
	VkImageType image_type) :
	Texture(ctx, format, tiling, usage_flags, mip_levels, array_layers, sample_count, image_type) {}

void Texture2D::load_from_img(const std::string& filename, VkSamplerCreateInfo* ci, bool generate_mipmaps) {
	int tex_width, tex_height, tex_size;
	uint8_t* img_data = nullptr;
	bool is_tex_format = has_extension(filename.c_str(), ".ktx") || has_extension(filename.c_str(), ".dds");
	std::unique_ptr<gli::texture2d> tex_2d;
	if(is_tex_format) {
		tex_2d = std::make_unique<gli::texture2d>(gli::load(filename.c_str()));
		img_data = static_cast<uint8_t*>(tex_2d->data());
		tex_width = (*tex_2d)[0].extent().x;
		tex_height = (*tex_2d)[0].extent().y;
		tex_size = static_cast<int>(tex_2d->size());
		mip_levels = static_cast<uint32_t>(tex_2d->levels());
	} else {
		int channels;
		img_data = stbi_load(filename.c_str(), &tex_width,
							 &tex_height, &channels, STBI_rgb_alpha);
		LUMEN_ASSERT(img_data, "Failed to load texture image");
		tex_size = tex_width * tex_height * channels;
	}
	Buffer staging_buffer;
	VkDeviceSize image_size = static_cast<uint64_t>(tex_size);

	// Create staging buffer
	staging_buffer.create(
		ctx,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		image_size,
		img_data
	);

	base_extent.width = tex_width;
	base_extent.height = tex_height;
	base_extent.depth = 1;

	// Need to do this check pre image creation
	if(generate_mipmaps) {
		usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		mip_levels = static_cast<uint32_t>(
			std::floor(std::log2(std::max(tex_width, tex_height)))) + 1;
	}
	this->create_image();

	std::vector<VkBufferImageCopy> regions;
	uint32_t existing_mip_levels = generate_mipmaps ? 1 : mip_levels;
	uint32_t offset = 0;
	for(uint32_t i = 0; i < existing_mip_levels; i++) {
		VkBufferImageCopy region{};

		region.bufferOffset = offset;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = i;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageExtent.width = is_tex_format ? (*tex_2d)[i].extent().x : base_extent.width >> i;
		region.imageExtent.height = is_tex_format ? (*tex_2d)[i].extent().y : base_extent.height >> i;
		region.imageExtent.depth = 1;
		regions.emplace_back(region);

		// having(.dds, .ktx) => more than one mip
		assert(tex_2d);
		offset += static_cast<uint32_t>((*tex_2d)[i].size());
	}

	// Copy from staging buffer to image
	VkImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource_range.baseArrayLayer = 0;
	subresource_range.layerCount = 1;
	subresource_range.baseMipLevel = 0;
	subresource_range.levelCount = mip_levels;

	CommandBuffer copy_cmd(ctx, true);
	transition_image_layout(copy_cmd.handle, img, VK_IMAGE_LAYOUT_UNDEFINED,
							VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresource_range);

	vkCmdCopyBufferToImage(
		copy_cmd.handle,
		staging_buffer.handle,
		img,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<uint32_t>(regions.size()),
		regions.data()
	);

	if(generate_mipmaps) {
		int mip_width = tex_width;
		int mip_height = tex_height;
		VkFormatProperties format_properties;
		vkGetPhysicalDeviceFormatProperties(ctx->physical_device, format, &format_properties);
		LUMEN_ASSERT((format_properties.optimalTilingFeatures &
					 VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT),
					 "Texture image format doesn't support linear blitting");

		VkImageSubresourceRange subresource_range = {};
		subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresource_range.baseArrayLayer = 0;
		subresource_range.layerCount = 1;
		subresource_range.levelCount = 1;
		for(uint32_t i = 1; i < mip_levels; i++) {
			subresource_range.baseMipLevel = i - 1;
			transition_image_layout(copy_cmd.handle, img,
									VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									subresource_range
			);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mip_width, mip_height, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mip_width > 1 ? mip_width >> 1 : 1, mip_height > 1 ? mip_height >> 1 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;
			vkCmdBlitImage(copy_cmd.handle,
						   img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						   1, &blit,
						   VK_FILTER_LINEAR);

			transition_image_layout(copy_cmd.handle, img,
									VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
									subresource_range);

			if(mip_width > 1) mip_width >>= 1;
			if(mip_height > 1) mip_height >>= 1;
		}
		subresource_range.baseMipLevel = mip_levels - 1;
		transition_image_layout(copy_cmd.handle, img,
								VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
								subresource_range);
	} else {
		transition_image_layout(copy_cmd.handle, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresource_range);
	}
	copy_cmd.submit();
	staging_buffer.destroy();
	img_view = create_image_view(ctx->device, img, format);
	if(!ci) {
		// Create a default sampler
		VkSamplerCreateInfo sampler_CI = vk::sampler_create_info();
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
		sampler_CI.maxLod = (float) mip_levels;
		sampler_CI.anisotropyEnable = ctx->supported_features.samplerAnisotropy;
		sampler_CI.maxAnisotropy = ctx->supported_features.samplerAnisotropy ?
			ctx->device_properties.limits.maxSamplerAnisotropy : 1.0f;

		sampler_CI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		vk::check(
			vkCreateSampler(ctx->device, &sampler_CI, nullptr, &sampler),
			"Could not create image sampler"
		);
	} else {
		vk::check(
			vkCreateSampler(ctx->device, ci, nullptr, &sampler),
			"Could not create image sampler"
		);
	}

	descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descriptor_image_info.imageView = img_view;
	descriptor_image_info.sampler = sampler;
	if(!is_tex_format) {
		stbi_image_free(img_data);
	}
}

void Texture2D::create_empty_texture(VulkanContext* ctx, const TextureSettings& settings, VkImageLayout img_layout,
									 VkImageAspectFlags flags/*=VK_IMAGE_ASPECT_COLOR_BIT*/) {
	this->ctx = ctx;
	this->format = settings.format;
	this->tiling = settings.tiling;
	this->usage_flags = settings.usage_flags;
	this->mip_levels = settings.mip_levels;
	this->array_layers = settings.array_layers;
	this->sample_count = settings.sample_count;
	this->base_extent = settings.base_extent;
	this->image_type = settings.image_type;

	create_image();
	img_view = create_image_view(ctx->device, img, settings.format, flags);
	// Create a default sampler
	// TODO: Select sampler from a sampler pool
	VkSamplerCreateInfo sampler_CI = vk::sampler_create_info();
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
	sampler_CI.anisotropyEnable = ctx->supported_features.samplerAnisotropy;
	sampler_CI.maxAnisotropy = ctx->supported_features.samplerAnisotropy ?
		ctx->device_properties.limits.maxSamplerAnisotropy : 1.0f;
	sampler_CI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	vk::check(
		vkCreateSampler(ctx->device, &sampler_CI, nullptr, &sampler),
		"Could not create image sampler"
	);
	descriptor_image_info.imageLayout = img_layout;
	descriptor_image_info.imageView = img_view;
	descriptor_image_info.sampler = sampler;
}

Texture::Texture(VulkanContext* ctx) : ctx(ctx) {}

Texture::Texture(VulkanContext* ctx, VkFormat format,
				 VkImageTiling tiling,
				 VkImageUsageFlags usage_flags,
				 uint32_t mip_levels,
				 uint32_t array_layers,
				 VkSampleCountFlagBits p_sample_count, VkImageType image_type) :
	ctx(ctx), format(format), tiling(tiling), usage_flags(usage_flags),
	mip_levels(mip_levels), array_layers(array_layers),
	sample_count(sample_count), base_extent(base_extent), image_type(image_type) {}


void Texture::create_image() {
	auto image_CI = vk::image_create_info(format, usage_flags, base_extent);
	image_CI.imageType = image_type;
	image_CI.mipLevels = mip_levels;
	image_CI.arrayLayers = array_layers;
	image_CI.tiling = tiling;
	image_CI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_CI.samples = sample_count;
	image_CI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	vk::check(
		vkCreateImage(ctx->device, &image_CI, nullptr, &img),
		"Failed to create image"
	);

	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements(ctx->device, img, &mem_req);
	auto alloc_info = vk::memory_allocate_info();
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type(&ctx->physical_device, mem_req.memoryTypeBits,
												  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vk::check(
		vkAllocateMemory(ctx->device, &alloc_info, nullptr, &img_mem),
		"Failed to allocate image memory"
	);
	vkBindImageMemory(ctx->device, img, img_mem, 0);
}

void Texture::destroy() {
	vkDestroySampler(ctx->device, sampler, nullptr);
	vkDestroyImageView(ctx->device, img_view, nullptr);
	vkDestroyImage(ctx->device, img, nullptr);
	vkFreeMemory(ctx->device, img_mem, nullptr);
}
