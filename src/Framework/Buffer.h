#pragma once
#include "LumenPCH.h"
struct Buffer {
	VkBuffer handle{};
	VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
	VulkanContext* ctx = nullptr;
	void* data = nullptr;
	VkDescriptorBufferInfo descriptor = {};
	VkDeviceSize size = 0;
	VkDeviceSize alignment = 0;
	VkBufferUsageFlags usage_flags = 0;
	VkMemoryPropertyFlags mem_property_flags = 0;

	inline void destroy() {
		if (handle)
			vkDestroyBuffer(ctx->device, handle, nullptr);
		if (buffer_memory)
			vkFreeMemory(ctx->device, buffer_memory, nullptr);
	}

	inline void bind(VkDeviceSize offset = 0) {
		vk::check(
			vkBindBufferMemory(ctx->device, handle, buffer_memory, offset),
			"Failed to bind buffer");
	}

	inline void map(VkDeviceSize size = VK_WHOLE_SIZE,
					VkDeviceSize offset = 0) {
		vk::check(
			vkMapMemory(ctx->device, buffer_memory, offset, size, 0, &data),
			"Unable to map memory");
	}

	inline void unmap() { vkUnmapMemory(ctx->device, buffer_memory); }

	inline VkDeviceAddress get_device_address() {
		VkBufferDeviceAddressInfo info = {
			VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
		info.buffer = handle;
		return vkGetBufferDeviceAddress(ctx->device, &info);
	}

	void create(VulkanContext*, VkBufferUsageFlags, VkMemoryPropertyFlags,
				VkSharingMode, VkDeviceSize, void* data = nullptr,
				bool use_staging = false);
	void flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void prepare_descriptor(VkDeviceSize size = VK_WHOLE_SIZE,
							VkDeviceSize offset = 0);
	void copy(Buffer& dst_buffer, VkCommandBuffer cmdbuf);
};
