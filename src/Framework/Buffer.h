#pragma once
#include "../LumenPCH.h"

namespace lumen {
class Buffer {
   public:
	void create(const char* name, VkBufferUsageFlags, VkMemoryPropertyFlags, VkDeviceSize,
				void* data = nullptr, bool use_staging = false, VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE);
	void create(VkBufferUsageFlags flags, VkMemoryPropertyFlags mem_property_flags,
					   VkDeviceSize size, void* data = nullptr, bool use_staging = false,
					   VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE);
	 void bind(VkDeviceSize offset = 0);
	 void map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	 void unmap();
	 VkDeviceAddress get_device_address();


	void flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void copy(Buffer& dst_buffer, VkCommandBuffer cmdbuf);
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
