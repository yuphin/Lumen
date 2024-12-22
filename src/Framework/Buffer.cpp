#include "../LumenPCH.h"
#include "Buffer.h"
#include <vulkan/vulkan_core.h>
#include "CommandBuffer.h"
#include "Framework/VulkanContext.h"
#include "VkUtils.h"
#include "VulkanContext.h"
#include "DynamicResourceManager.h"
#include "VulkanStructs.h"

namespace vk {
void create_buffer(Buffer* buffer, const BufferDesc& desc) {
	buffer->name = desc.name;
	buffer->size = desc.size;
	buffer->usage_flags = desc.usage;

	VmaAllocationCreateInfo alloc_ci = {};
	alloc_ci.usage = VMA_MEMORY_USAGE_AUTO;
	if (int(desc.memory_type & BufferType::GPU_TO_CPU) != 0) {
		alloc_ci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	} else if (int(desc.memory_type & BufferType::CPU_TO_GPU) != 0) {
		alloc_ci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
						  VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
	}
	if (desc.memory_type == BufferType::STAGING) {
		alloc_ci.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	} else if (desc.data && desc.memory_type == BufferType::GPU) {
		// In case we need to copy data to the buffer
		buffer->usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	if (buffer->usage_flags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT) {
		alloc_ci.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
	}
	if(desc.dedicated_allocation) {
		alloc_ci.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	}
	VkBufferCreateInfo buffer_ci = vk::buffer(buffer->usage_flags, buffer->size, VK_SHARING_MODE_EXCLUSIVE);
	VmaAllocationInfo alloc_info;
	vk::check(vmaCreateBuffer(vk::context().allocator, &buffer_ci, &alloc_ci, &buffer->handle, &buffer->allocation, &alloc_info));
	if (!buffer->name.empty()) {
		vk::DebugMarker::set_resource_name(vk::context().device, (uint64_t)buffer->handle, buffer->name.data(),
										   VK_OBJECT_TYPE_BUFFER);
	}
	VkMemoryPropertyFlags mem_prop_flags;
	vmaGetAllocationMemoryProperties(vk::context().allocator, buffer->allocation, &mem_prop_flags);
	if (desc.data && (mem_prop_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0) {
		Buffer* staging_buffer = drm::get({.name = "Scratch Buffer",
										   .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
										   .memory_type = BufferType::STAGING,
										   .size = buffer->size,
										   .data = desc.data,
										   .dedicated_allocation = false});
		vk::CommandBuffer cmd(true);
		VkBufferCopy copy_region = {
			.size = buffer->size,
		};
		vkCmdCopyBuffer(cmd.handle, staging_buffer->handle, buffer->handle, 1, &copy_region);
		cmd.submit(vk::context().queues[0]);
		drm::destroy(staging_buffer);

	} else if (desc.data) {
		memcpy(alloc_info.pMappedData, desc.data, buffer->size);
		vk::check(vmaFlushAllocation(vk::context().allocator, buffer->allocation, 0, buffer->size));
	}
}

VkDescriptorBufferInfo get_buffer_descriptor(const vk::Buffer* buffer) {
	VkDescriptorBufferInfo buffer_info = {};
	buffer_info.buffer = buffer->handle;
	buffer_info.offset = 0;
	buffer_info.range = buffer->size;
	return buffer_info;
}

void destroy_buffer(Buffer* buffer) { vmaDestroyBuffer(vk::context().allocator, buffer->handle, buffer->allocation); }

void write_buffer(Buffer* buffer, void* data, size_t size) {
	VkMemoryPropertyFlags mem_prop_flags;
	vmaGetAllocationMemoryProperties(vk::context().allocator, buffer->allocation, &mem_prop_flags);
	LUMEN_ASSERT((mem_prop_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0, "Buffer is not host visible");
	void* memory;
	vmaMapMemory(vk::context().allocator, buffer->allocation, &memory);
	memcpy(memory, data, size);
	vmaUnmapMemory(vk::context().allocator, buffer->allocation);
	// Flush ignored if the buffer is host coherent
	vmaFlushAllocation(vk::context().allocator, buffer->allocation, 0, size);
}

void* map_buffer(Buffer* buffer) {
	VkMemoryPropertyFlags mem_prop_flags;
	vmaGetAllocationMemoryProperties(vk::context().allocator, buffer->allocation, &mem_prop_flags);
	LUMEN_ASSERT((mem_prop_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0, "Buffer is not host visible");
	void* memory;
	vmaMapMemory(vk::context().allocator, buffer->allocation, &memory);
	return memory;
}
void unmap_buffer(Buffer* buffer)  {
	vmaUnmapMemory(vk::context().allocator, buffer->allocation);
}

}  // namespace vk