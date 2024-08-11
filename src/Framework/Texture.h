#pragma once
#include <vulkan/vulkan_core.h>
#include "../LumenPCH.h"

namespace vk {
struct TextureData {
	void* data = nullptr;
	VkDeviceSize size = 0;
};
struct TextureDesc {
	std::string_view name = "";
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
	uint32_t num_mips = 1;
	uint32_t array_layers = 1;
	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	VkFilter sampler_filter = VK_FILTER_LINEAR;
	VkSamplerAddressMode sampler_address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkSampler sampler = VK_NULL_HANDLE;
	VkImage image = nullptr;
};
struct Texture {
	std::string_view name;
	VkImage handle;
	VkExtent3D extent;
	VkImageView view;
	VkSampler sampler;
	VkFormat format;
	VkImageUsageFlags usage_flags;
	VkImageLayout layout;
	VkImageAspectFlags aspect_flags;
	uint32_t mip_levels;
	uint32_t array_layers;
	VmaAllocation allocation = VK_NULL_HANDLE;
};

void create_texture(Texture* texture, const TextureDesc& desc);
void destroy_texture(Texture* texture);

VkDescriptorImageInfo get_texture_descriptor(const Texture* tex, VkSampler sampler, VkImageLayout layout);
VkDescriptorImageInfo get_texture_descriptor(const Texture* tex, VkImageLayout layout);
VkDescriptorImageInfo get_texture_descriptor(const Texture* tex, VkSampler sampler);
VkDescriptorImageInfo get_texture_descriptor(const Texture* tex);
void force_transition_texture(Texture* tex, VkCommandBuffer cmd, VkImageLayout old_layout, VkImageLayout new_layout);
void transition_texture(Texture* tex, VkCommandBuffer cmd, VkImageLayout new_layout);

}  // namespace vk
