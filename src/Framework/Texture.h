#pragma once
#include "LumenPCH.h"

struct TextureSettings {
	VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
	VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
	VkImageUsageFlags usage_flags =
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	uint32_t mip_levels = 1;
	uint32_t array_layers = 1;
	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	VkExtent3D base_extent = { 0, 0, 1 };
	VkImageType image_type = VK_IMAGE_TYPE_2D;
};

class Texture {
public:
	Texture() = default;
	Texture(VulkanContext*);
	Texture(VulkanContext*, VkFormat, VkImageTiling, VkImageUsageFlags,
			uint32_t mip_levels, uint32_t array_layers, VkSampleCountFlagBits,
			VkImageType);
	VkImage img = VK_NULL_HANDLE;
	VkImageView img_view = VK_NULL_HANDLE;
	VkDeviceMemory img_mem = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VkDescriptorImageInfo descriptor_image_info = {};
	void destroy();
	inline void set_context(VulkanContext* ctx) { this->ctx = ctx; }

	VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
	VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
	VkImageUsageFlags usage_flags =
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	uint32_t mip_levels = 1;
	uint32_t array_layers = 1;
	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	VkExtent3D base_extent = { 0, 0, 0 };
	VkImageType image_type = VK_IMAGE_TYPE_2D;
	VulkanContext* ctx;
	VkImageLayout layout;
	bool sampler_allocated = false;

protected:
	void create_image(const VkImageCreateInfo& info);
	void cmd_generate_mipmaps(int mip_width, int mip_height,
							  VkCommandBuffer cmd);
};

class Texture2D : public Texture {
public:
	Texture2D() = default;
	Texture2D(VulkanContext*);
	Texture2D(VulkanContext*, VkFormat, VkImageTiling, VkImageUsageFlags,
			  uint32_t mip_levels, uint32_t array_layers, VkSampleCountFlagBits,
			  VkImageType);
	void load_from_img(const std::string& filename, VulkanContext* ctx,
					   VkSampler, VkSamplerCreateInfo* = nullptr,
					   bool generate_mipmaps = true);

	void load_from_data(VulkanContext* ctx, void* data, VkDeviceSize size,
						const VkImageCreateInfo& info, VkSampler a_sampler,
						bool generate_mipmaps = true);
	void
		create_empty_texture(VulkanContext* ctx, const TextureSettings& settings,
							 VkImageLayout img_layout,
							 VkImageAspectFlags flags = VK_IMAGE_ASPECT_COLOR_BIT);

private:
};
