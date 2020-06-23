#pragma once
#include <vulkan/vulkan.h>
#include "VKUtils.h"
struct Buffer {
	VkBuffer handle = VK_NULL_HANDLE;
	VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
	VkDevice device;
	void* data;
	VkDescriptorBufferInfo descriptor;
	VkDeviceSize size = 0;
	VkDeviceSize alignment = 0;
	VkBufferUsageFlags usage_flags;
	VkMemoryPropertyFlags mem_property_flags;

	void destroy() {
		if (handle) vkDestroyBuffer(device, handle, nullptr);
		if (buffer_memory) vkFreeMemory(device, buffer_memory, nullptr);
	}

	void bind(VkDeviceSize offset = 0) {
		check(vkBindBufferMemory(device, handle, buffer_memory, offset), "Failed to bind buffer");

	}

	void map_memory(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) {
		 check(vkMapMemory(device, buffer_memory, offset, size, 0, &data), "Unable to map memory");
	}

	void unmap() {
		vkUnmapMemory(device, buffer_memory);
	}

	void flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) {
		VkMappedMemoryRange mapped_range = {};
		mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mapped_range.memory = buffer_memory;
		mapped_range.offset = offset;
		mapped_range.size = size;
		check(vkFlushMappedMemoryRanges(device, 1, &mapped_range), "Failed to flush mapped memory ranges");
	}

	void invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) {
		VkMappedMemoryRange mapped_range = {};
		mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		mapped_range.memory = buffer_memory;
		mapped_range.offset = offset;
		mapped_range.size = size;
		check(vkInvalidateMappedMemoryRanges(device, 1, &mapped_range),
			"Failed to invalidate mapped memory range");
	}

	void prepare_descriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) {
		descriptor.offset = offset;
		descriptor.buffer = handle;
		descriptor.range = size;
	}
};

