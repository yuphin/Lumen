#include "lmhpch.h"
#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "gfx/vulkan/Utils.h"
#include "gfx/vulkan/CommandBuffer.h"



Texture2D::Texture2D(
	VkFormat format,
	VkImageTiling tiling,
	VkImageUsageFlags usage_flags,
	uint32_t mip_levels,
	uint32_t array_layers,
	VkSampleCountFlagBits sample_count,
	VkImageType image_type) :
	Texture(format, tiling, usage_flags, mip_levels, array_layers, sample_count, image_type) {}

void Texture2D::load_from_file(const std::string& filename, VkSamplerCreateInfo* ci, bool generate_mipmaps) {
	int tex_width, tex_height, channels;
	auto* img_data = stbi_load(filename.c_str(), &tex_width,
		&tex_height, &channels, STBI_rgb_alpha);
	LUMEN_ASSERT(img_data, "Failed to load texture image");

	Buffer staging_buffer;
	VkDeviceSize image_size = static_cast<uint64_t>(tex_width) * tex_height * 4;
	// Need to do this check pre image creation
	if (generate_mipmaps) {
		usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		mip_levels = static_cast<uint32_t>(
			std::floor(std::log2(std::max(tex_width, tex_height)))) + 1;
	}
	// Create staging buffer
	create_buffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		VK_SHARING_MODE_EXCLUSIVE,
		staging_buffer,
		image_size,
		img_data
	);
	// We can remove old pixel data
	stbi_image_free(img_data);

	extent.width = tex_width;
	extent.height = tex_height;
	extent.depth = 1;

	create_image();


	// Copy from staging buffer to image
	VkImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource_range.baseArrayLayer = 0;
	subresource_range.layerCount = array_layers;
	subresource_range.baseMipLevel = 0;
	subresource_range.levelCount = mip_levels;
	std::vector<VkBufferImageCopy> regions;
	uint32_t existing_mip_levels = generate_mipmaps ? 1 : mip_levels;
	for (uint32_t i = 0; i < existing_mip_levels; i++) {
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = i;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageExtent.width = extent.width >> i;
		region.imageExtent.height = extent.height >> i;
		region.imageExtent.depth = 1;
		regions.emplace_back(region);
	}
	CommandBuffer copy_cmd(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
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
	if (generate_mipmaps) {
		int mip_width = tex_width;
		int mip_height = tex_height;
		VkFormatProperties format_properties;
		vkGetPhysicalDeviceFormatProperties(Lumen::get()->get_physical_device(), format, &format_properties);
		LUMEN_ASSERT((format_properties.optimalTilingFeatures &
			VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT),
			"Texture image format doesn't support linear blitting");
		VkImageSubresourceRange subresource_range = {};
		subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresource_range.baseArrayLayer = 0;
		subresource_range.layerCount = array_layers;
		subresource_range.levelCount = 1;
		for (uint32_t i = 1; i < mip_levels; i++) {
			subresource_range.baseMipLevel = i - 1;
			transition_image_layout(copy_cmd.handle, img,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				subresource_range);

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

			if (mip_width > 1) mip_width >>= 1;
			if (mip_height > 1) mip_height >>= 1;
		}
		subresource_range.baseMipLevel = mip_levels - 1;
		transition_image_layout(copy_cmd.handle, img,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresource_range);
	}
	else {
		transition_image_layout(copy_cmd.handle, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresource_range);
	}
	const auto& gfx_queue = Lumen::get()->get_gfx_queue();
	copy_cmd.submit(gfx_queue);
	staging_buffer.destroy();
	img_view = create_image_view(img, format);
	const auto& device = Lumen::get()->get_device();
	if (!ci) {
		// Create a default sampler
		const auto& device_properties = Lumen::get()->get_device_properties();
		const auto& device_features = Lumen::get()->get_supported_features();
		VkSamplerCreateInfo sampler_CI = vks::sampler_create_info();
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
		sampler_CI.maxLod = (float)mip_levels;
		sampler_CI.anisotropyEnable = device_features.samplerAnisotropy;
		sampler_CI.maxAnisotropy = device_features.samplerAnisotropy ?
			device_properties.limits.maxSamplerAnisotropy : 1.0f;

		sampler_CI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		vks::check(
			vkCreateSampler(device, &sampler_CI, nullptr, &sampler),
			"Could not create image sampler"
		);
	}
	else {
		vks::check(
			vkCreateSampler(device, ci, nullptr, &sampler),
			"Could not create image sampler"
		);
	}

	descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descriptor_image_info.imageView = img_view;
	descriptor_image_info.sampler = sampler;

}

Texture::Texture(VkFormat format,
	VkImageTiling iling,
	VkImageUsageFlags usage_flags,
	uint32_t mip_levels,
	uint32_t array_layers,
	VkSampleCountFlagBits p_sample_count, VkImageType image_type) :
	format(format), tiling(tiling), usage_flags(usage_flags),
	mip_levels(mip_levels), array_layers(array_layers),
	sample_count(sample_count), extent(extent), image_type(image_type) { }


void Texture::create_image() {
	auto image_CI = vks::image_create_info();
	const auto& device = Lumen::get()->get_device();
	image_CI.imageType = image_type;
	image_CI.extent = extent;
	image_CI.mipLevels = mip_levels;
	image_CI.arrayLayers = array_layers;
	image_CI.format = format;
	image_CI.tiling = tiling;
	image_CI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_CI.usage = usage_flags;
	image_CI.samples = sample_count;
	image_CI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	vks::check(
		vkCreateImage(device, &image_CI, nullptr, &img),
		"Failed to create image"
	);

	VkMemoryRequirements mem_req;
	vkGetImageMemoryRequirements(device, img, &mem_req);
	auto alloc_info = vks::memory_allocate_info();
	alloc_info.allocationSize = mem_req.size;
	alloc_info.memoryTypeIndex = find_memory_type(mem_req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vks::check(
		vkAllocateMemory(device, &alloc_info, nullptr, &img_mem),
		"Failed to allocate image memory"
	);
	vkBindImageMemory(device, img, img_mem, 0);


}

void Texture::blit_mipmaps() {


}

void Texture::destroy() {
	const auto& device = Lumen::get()->get_device();
	vkDestroySampler(device, sampler, nullptr);
	vkDestroyImageView(device, img_view, nullptr);
	vkDestroyImage(device, img, nullptr);
	vkFreeMemory(device, img_mem, nullptr);
}

