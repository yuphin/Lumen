#pragma once
#include "LumenPCH.h"

struct TextureSettings {
	VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
	VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
	VkImageUsageFlags usage_flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	uint32_t mip_levels = 1;
	uint32_t array_layers = 1;
	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	VkExtent3D base_extent = {0, 0, 1};
	VkImageType image_type = VK_IMAGE_TYPE_2D;
	bool present = false;
};

class Texture {
   public:
	Texture() = default;
	Texture(VulkanContext*);
	// Texture(VulkanContext*, VkFormat, VkImageTiling, VkImageUsageFlags,
	//		uint32_t mip_levels, uint32_t array_layers, VkSampleCountFlagBits,
	//		VkImageType);
	void destroy();
	inline void set_context(VulkanContext* ctx) { this->ctx = ctx; }
	VkImage img = VK_NULL_HANDLE;
	VkImageView img_view = VK_NULL_HANDLE;
	VkDeviceMemory img_mem = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VulkanContext* ctx;

	// VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
	VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
	VkImageUsageFlags usage_flags = 0;
	uint32_t mip_levels = 1;
	// uint32_t array_layers = 1;
	// VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
	 VkExtent3D base_extent = { 0, 0, 0 };
	// VkImageType image_type = VK_IMAGE_TYPE_2D;
	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
	bool sampler_allocated = false;
	bool present = false;
	VkImageAspectFlags aspect_flags;
	std::string name = "";

   protected:
	void create_image(const VkImageCreateInfo& info);
	void cmd_generate_mipmaps(const VkImageCreateInfo& info, VkCommandBuffer cmd);
};

class Texture2D : public Texture {
   public:
	Texture2D() = default;
	Texture2D(VulkanContext*);

	Texture2D(const std::string& name, VulkanContext* ctx, VkImage image, VkFormat format, VkImageUsageFlags flags,
			  VkImageAspectFlags aspect_flags, bool present = true);
	void load_from_data(VulkanContext* ctx, void* data, VkDeviceSize size, const VkImageCreateInfo& info,
						VkSampler a_sampler, VkImageUsageFlags flags, bool generate_mipmaps = false);
	void create_empty_texture(const char* name, VulkanContext* ctx, const TextureSettings& settings,
							  VkImageLayout img_layout, VkSampler = 0,
							  VkImageAspectFlags flags = VK_IMAGE_ASPECT_COLOR_BIT);
	inline void create_empty_texture(VulkanContext* ctx, const TextureSettings& settings, VkImageLayout img_layout,
									 VkSampler sampler = 0, VkImageAspectFlags flags = VK_IMAGE_ASPECT_COLOR_BIT) {
		return create_empty_texture("", ctx, settings, img_layout, sampler, flags);
	}

	void transition(VkCommandBuffer cmd, VkImageLayout new_layout);
	void force_transition(VkCommandBuffer cmd, VkImageLayout old_layout, VkImageLayout new_layout);
	void transition_without_state(VkCommandBuffer cmd, VkImageLayout new_layout);
	VkDescriptorImageInfo descriptor(VkSampler sampler, VkImageLayout layout) const;
	VkDescriptorImageInfo descriptor(VkImageLayout layout) const;
	VkDescriptorImageInfo descriptor(VkSampler sampler) const;
	VkDescriptorImageInfo descriptor() const;

   private:
};
