#pragma once
#include <vulkan/vulkan_core.h>
#include "../LumenPCH.h"

// namespace lumen {
// struct TextureSettings {
// 	VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
// 	VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
// 	VkImageUsageFlags usage_flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
// 	uint32_t mip_levels = 1;
// 	uint32_t array_layers = 1;
// 	VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT;
// 	VkExtent3D base_extent = {0, 0, 1};
// 	VkImageType image_type = VK_IMAGE_TYPE_2D;
// 	bool present = false;
// };

// class Texture {
//    public:
// 	Texture() = default;
// 	void destroy();
// 	inline bool valid() { return img != VK_NULL_HANDLE; }
// 	VkImage img = VK_NULL_HANDLE;
// 	VkImageView img_view = VK_NULL_HANDLE;
// 	VkDeviceMemory img_mem = VK_NULL_HANDLE;
// 	VkSampler sampler = VK_NULL_HANDLE;

// 	VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
// 	VkImageUsageFlags usage_flags = 0;
// 	uint32_t mip_levels = 1;
// 	VkExtent3D base_extent = {0, 0, 0};
// 	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
// 	bool sampler_allocated = false;
// 	bool present = false;
// 	VkImageAspectFlags aspect_flags;
// 	std::string name = "";

//    protected:
// 	void create_image(const VkImageCreateInfo& info);
// 	void cmd_generate_mipmaps(const VkImageCreateInfo& info, VkCommandBuffer cmd);
// };

// class Texture2D : public Texture {
//    public:
// 	Texture2D() = default;
// 	Texture2D(const std::string& name, VkImage image, VkFormat format, VkImageUsageFlags flags,
// 			  VkImageAspectFlags aspect_flags, VkExtent2D extent, bool present = true);
// 	void load_from_data(void* data, VkDeviceSize size, const VkImageCreateInfo& info, VkSampler a_sampler,
// 						VkImageUsageFlags flags, bool generate_mipmaps = false);
// 	void create_empty_texture(const char* name, const TextureSettings& settings, VkImageLayout img_layout,
// 							  VkSampler = 0, VkImageAspectFlags flags = VK_IMAGE_ASPECT_COLOR_BIT);
// 	inline void create_empty_texture(const TextureSettings& settings, VkImageLayout img_layout, VkSampler sampler = 0,
// 									 VkImageAspectFlags flags = VK_IMAGE_ASPECT_COLOR_BIT) {
// 		return create_empty_texture("", settings, img_layout, sampler, flags);
// 	}

// 	void transition(VkCommandBuffer cmd, VkImageLayout new_layout);
// 	void force_transition(VkCommandBuffer cmd, VkImageLayout old_layout, VkImageLayout new_layout);
// 	void transition_without_state(VkCommandBuffer cmd, VkImageLayout new_layout);
// 	VkDescriptorImageInfo descriptor(VkSampler sampler, VkImageLayout layout) const;
// 	VkDescriptorImageInfo descriptor(VkImageLayout layout) const;
// 	VkDescriptorImageInfo descriptor(VkSampler sampler) const;
// 	VkDescriptorImageInfo descriptor() const;

//    private:
// };

// }  // namespace lumen

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
