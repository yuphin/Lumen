#pragma once
#include "lmhpch.h"
struct Buffer {
	VkBuffer handle = VK_NULL_HANDLE;
	VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	void* data = nullptr;
	VkDescriptorBufferInfo descriptor = {};
	VkDeviceSize size = 0;
	VkDeviceSize alignment = 0;
	VkBufferUsageFlags usage_flags = 0;
	VkMemoryPropertyFlags mem_property_flags = 0;

	inline void destroy() {
		if (handle) vkDestroyBuffer(device, handle, nullptr);
		if (buffer_memory) vkFreeMemory(device, buffer_memory, nullptr);
	}

	inline void bind(VkDeviceSize offset = 0) {
		vks::check(vkBindBufferMemory(device, handle, buffer_memory, offset), "Failed to bind buffer");

	}

	inline void map_memory(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) {
		vks::check(vkMapMemory(device, buffer_memory, offset, size, 0, &data), "Unable to map memory");
	}

	inline void unmap() {
		vkUnmapMemory(device, buffer_memory);
	}

	void flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void prepare_descriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

};

