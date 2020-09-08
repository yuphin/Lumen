#pragma once
#include "lmhpch.h"
#include "gfx/vulkan/Base.h"

class Texture {
public:
	Texture() = default;
	Texture(VkFormat, VkImageTiling, VkImageUsageFlags, uint32_t mip_levels,
		uint32_t array_layers, VkSampleCountFlagBits, VkImageType);
	VkImage img = VK_NULL_HANDLE;
	VkImageView img_view = VK_NULL_HANDLE;
	VkDeviceMemory img_mem = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VkDescriptorImageInfo descriptor_image_info = {};
	void destroy();
protected:
	void create_image();
	void blit_mipmaps();
	VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
	VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
	VkImageUsageFlags usage_flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	uint32_t mip_levels = 1;
	uint32_t array_layers = 1;
	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	VkExtent3D extent = { 0, 0, 0 };
	VkImageType image_type = VK_IMAGE_TYPE_2D;

};

class Texture2D : public Texture {
public:
	Texture2D() = default;
	Texture2D(VkFormat, VkImageTiling, VkImageUsageFlags, uint32_t mip_levels,
		uint32_t array_layers, VkSampleCountFlagBits, VkImageType);
	void load_from_file(const std::string& filename,
		VkSamplerCreateInfo* = nullptr, bool generate_mipmaps = false);
};

