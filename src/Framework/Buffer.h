#pragma once
#include "VulkanContext.h"
namespace vk {

enum BufferType : uint32_t {
    BUFFER_TYPE_GPU        = 1u << 0,
    BUFFER_TYPE_GPU_TO_CPU = 1u << 1,
    BUFFER_TYPE_CPU_TO_GPU = 1u << 2,
    BUFFER_TYPE_STAGING    = 1u << 3,
};

using BufferFlags = u32;

struct BufferStatus {
	bool read = false;
	bool write = false;
};

struct BufferDesc {
	std::string_view name = "";
	VkBufferUsageFlags usage;
	BufferType memory_type;
	VkDeviceSize size;
	bool create_mapped = false;
	void* data = nullptr;
	// For now this is set to true by default
	// Otherwise on state changes the previous memory data is *spilled* to the persistent memory
	// and some integrators assume that the memory is allocated from a dedicated pool 

	// For buffers allocated from the DynamicResourceManager this can be explicitly set to false
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

void buffer_create(Buffer* buffer, const BufferDesc& desc);
VkDescriptorBufferInfo buffer_descriptor(const Buffer* buffer);
void buffer_destroy(Buffer* buffer);
void write_buffer(Buffer* buffer, void* data, u64 size);
void* buffer_map(Buffer* buffer);
void buffer_unmap(Buffer* buffer);

}  // namespace vk