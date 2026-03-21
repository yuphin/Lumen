#pragma once
#include "Base/String.h"

namespace vk {
struct TextureData {
	void* data = nullptr;
	VkDeviceSize size = 0;
};
struct TextureDesc {
	lm::String name;
	VkImageUsageFlags usage;
	VkExtent3D dimensions;
	VkFormat format;
	// Optional settings
	VkImageLayout initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	struct {
		void* data = nullptr;
		VkDeviceSize size = 0;
	} data;
	VkImageType image_type = VK_IMAGE_TYPE_2D;
	VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
	bool calc_mips = false;
	u32 num_mips = 1;
	u32 array_layers = 1;
	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	VkFilter sampler_filter = VK_FILTER_LINEAR;
	VkSamplerAddressMode sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkSampler sampler = VK_NULL_HANDLE;
	VkImage image = nullptr;
};
struct Texture {
	lm::String name;
	VkImage handle;
	VkExtent3D extent;
	VkImageView view;
	VkSampler sampler;
	VkFormat format;
	VkImageUsageFlags usage_flags;
	VkImageLayout layout;
	VkImageAspectFlags aspect_flags;
	u32 mip_levels;
	u32 array_layers;
	VmaAllocation allocation = VK_NULL_HANDLE;
};

void texture_create(Texture* texture, const TextureDesc& desc);
void texture_destroy(Texture* texture);

VkDescriptorImageInfo texture_descriptor(const Texture* tex, VkSampler sampler, VkImageLayout layout);
VkDescriptorImageInfo texture_descriptor(const Texture* tex, VkImageLayout layout);
VkDescriptorImageInfo texture_descriptor(const Texture* tex, VkSampler sampler);
VkDescriptorImageInfo texture_descriptor(const Texture* tex);
void texture_force_transition(Texture* tex, VkCommandBuffer cmd, VkImageLayout old_layout, VkImageLayout new_layout);
void texture_transition(Texture* tex, VkCommandBuffer cmd, VkImageLayout new_layout);
VkImageLayout texture_to_image_layout(const Texture* tex, VkAccessFlags access_flags);

}  // namespace vk
