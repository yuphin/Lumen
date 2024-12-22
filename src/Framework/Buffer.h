#pragma once
#include "../LumenPCH.h"
#include "EnumFlags.h"
#include "VulkanContext.h"
namespace vk {

enum class BufferType { GPU = 1 << 0, GPU_TO_CPU = 1 << 1, CPU_TO_GPU = 1 << 2, STAGING = 1 << 3 };
DEFINE_ENUM_FLAGS(BufferType)

struct BufferDesc {
	std::string_view name = "";
	VkBufferUsageFlags usage;
	BufferType memory_type;
	VkDeviceSize size;
	void* data = nullptr;
	// For now this is set to true by default
	// Otherwise on state changes the previous memory data is *spilled*
	// and some integrators assume that the memory is allocated from a dedicated pool 
	bool dedicated_allocation = true;
};

struct Buffer {
	std::string_view name;
	VkBuffer handle{};
	VkDeviceSize size = 0;
	VkBufferUsageFlags usage_flags = 0;
	VmaAllocation allocation = VK_NULL_HANDLE;

	VkDeviceAddress get_device_address() const {
		VkBufferDeviceAddressInfo info = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = handle};
		return vkGetBufferDeviceAddress(vk::context().device, &info);
	}
};

void create_buffer(Buffer* buffer, const BufferDesc& desc);
VkDescriptorBufferInfo get_buffer_descriptor(const Buffer* buffer);
void destroy_buffer(Buffer* buffer);
void write_buffer(Buffer* buffer, void* data, size_t size);
void* map_buffer(Buffer* buffer);
void unmap_buffer(Buffer* buffer);

}  // namespace vk