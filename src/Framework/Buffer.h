#pragma once
#include "../LumenPCH.h"
#include "EnumFlags.h"
#include "VulkanContext.h"

namespace lumen {
class BufferOld {
   public:
	void create(const char* name, VkBufferUsageFlags, VkMemoryPropertyFlags, VkDeviceSize, void* data = nullptr,
				bool use_staging = false, VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE);
	void create(VkBufferUsageFlags flags, VkMemoryPropertyFlags mem_property_flags, VkDeviceSize size,
				void* data = nullptr, bool use_staging = false, VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE);
	void bind(VkDeviceSize offset = 0);
	void map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void unmap();
	VkDeviceAddress get_device_address();

	void flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void copy(BufferOld& dst_buffer, VkCommandBuffer cmdbuf);
	void destroy();

	VkBuffer handle{};
	VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
	void* data = nullptr;
	VkDescriptorBufferInfo descriptor = {};
	VkDeviceSize size = 0;
	VkDeviceSize alignment = 0;
	VkBufferUsageFlags usage_flags = 0;
	VkMemoryPropertyFlags mem_property_flags = 0;
	std::string name;

   private:
	void prepare_descriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
};

}  // namespace lumen

namespace vk {

enum class BufferUsage : VkBufferUsageFlags {
	STORAGE = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
	UNIFORM = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	DEVICE_ADDRESS = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
	TRANSFER_SRC = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	TRANSFER_DST = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	INDEX = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	ACCEL = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,

};
DEFINE_ENUM_FLAGS(BufferUsage)

enum class BufferType { GPU = 1 << 0, GPU_TO_CPU = 1 << 1, CPU_TO_GPU = 1 << 2, STAGING = 1 << 3 };
DEFINE_ENUM_FLAGS(BufferType)

struct BufferDesc {
	std::string_view name;
	VkBufferUsageFlags usage;
	BufferType memory_type;
	VkDeviceSize size;
	void* data = nullptr;
};

struct Buffer {
	std::string_view name;
	VkBuffer handle{};
	VkDeviceSize size = 0;
	VkBufferUsageFlags usage_flags = 0;
	VmaAllocation allocation = VK_NULL_HANDLE;
};

void create_buffer(Buffer* buffer, const BufferDesc& desc);
VkDescriptorBufferInfo get_descriptor_buffer_info(const Buffer& buffer);
void destroy_buffer(Buffer* buffer);

}  // namespace vk