#include "../LumenPCH.h"
#include "Buffer.h"
#include <vulkan/vulkan_core.h>
#include "CommandBuffer.h"
#include "Framework/VulkanContext.h"
#include "VkUtils.h"
#include "VulkanContext.h"
#include "DynamicResourceManager.h"
#include "VulkanStructs.h"

// namespace lumen {

// void BufferOld::create(const char* name, VkBufferUsageFlags usage, VkMemoryPropertyFlags mem_property_flags,
// 					   VkDeviceSize size, void* data, bool use_staging, VkSharingMode sharing_mode) {
// 	this->mem_property_flags = mem_property_flags;
// 	this->usage_flags = usage;

// 	if (use_staging) {
// 		BufferOld staging_buffer;

// 		LUMEN_ASSERT(mem_property_flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "Buffer creation error");
// 		staging_buffer.create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
// 							  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, size, data);

// 		staging_buffer.unmap();
// 		create(VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, mem_property_flags, size, nullptr, false, sharing_mode);

// 		CommandBuffer copy_cmd(true);
// 		VkBufferCopy copy_region = {};

// 		copy_region.size = size;
// 		vkCmdCopyBuffer(copy_cmd.handle, staging_buffer.handle, this->handle, 1, &copy_region);
// 		copy_cmd.submit(vk::context().queues[0]);
// 		staging_buffer.destroy();
// 	} else {
// 		// Create the buffer handle
// 		VkBufferCreateInfo buffer_CI = vk::buffer(usage, size, sharing_mode);
// 		vk::check(vkCreateBuffer(vk::context().device, &buffer_CI, nullptr, &this->handle),
// 				  "Failed to create vertex buffer!");

// 		// Create the memory backing up the buffer handle
// 		VkMemoryRequirements mem_reqs;
// 		VkMemoryAllocateInfo mem_alloc_info = vk::memory_allocate_info();
// 		vkGetBufferMemoryRequirements(vk::context().device, this->handle, &mem_reqs);

// 		mem_alloc_info.allocationSize = mem_reqs.size;
// 		// Find a memory type index that fits the properties of the buffer
// 		mem_alloc_info.memoryTypeIndex =
// 			vk::find_memory_type(&vk::context().physical_device, mem_reqs.memoryTypeBits, mem_property_flags);

// 		VkMemoryAllocateFlagsInfo flags_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
// 		if (usage_flags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
// 			flags_info.flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
// 			mem_alloc_info.pNext = &flags_info;
// 		}
// 		vk::check(vkAllocateMemory(vk::context().device, &mem_alloc_info, nullptr, &this->buffer_memory),
// 				  "Failed to allocate buffer memory!");

// 		alignment = mem_reqs.alignment;
// 		this->size = size;
// 		usage_flags = usage;
// 		if (mem_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
// 			map();
// 		}

// 		// If a pointer to the buffer data has been passed, map the buffer and
// 		// copy over the data
// 		if (data != nullptr) {
// 			// Memory is assumed mapped at this point
// 			memcpy(this->data, data, size);
// 			if ((mem_property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
// 				flush();
// 			}
// 		}

// 		// Initialize a default descriptor that covers the whole buffer size
// 		prepare_descriptor();
// 		bind();
// 	}
// 	if (name) {
// 		vk::DebugMarker::set_resource_name(vk::context().device, (uint64_t)handle, name, VK_OBJECT_TYPE_BUFFER);
// 		this->name = name;
// 	}
// }
// void BufferOld::flush(VkDeviceSize size, VkDeviceSize offset) {
// 	VkMappedMemoryRange mapped_range = {};
// 	mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
// 	mapped_range.memory = buffer_memory;
// 	mapped_range.offset = offset;
// 	mapped_range.size = size;
// 	vk::check(vkFlushMappedMemoryRanges(vk::context().device, 1, &mapped_range),
// 			  "Failed to flush mapped memory ranges");
// }

// void BufferOld::invalidate(VkDeviceSize size, VkDeviceSize offset) {
// 	VkMappedMemoryRange mapped_range = {};
// 	mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
// 	mapped_range.memory = buffer_memory;
// 	mapped_range.offset = offset;
// 	mapped_range.size = size;
// 	vk::check(vkInvalidateMappedMemoryRanges(vk::context().device, 1, &mapped_range),
// 			  "Failed to invalidate mapped memory range");
// }

// void BufferOld::prepare_descriptor(VkDeviceSize size, VkDeviceSize offset) {
// 	descriptor.offset = offset;
// 	descriptor.buffer = handle;
// 	descriptor.range = size;
// }

// void BufferOld::copy(BufferOld& dst_buffer, VkCommandBuffer cmdbuf) {
// 	VkBufferCopy copy_region;
// 	copy_region.srcOffset = 0;
// 	copy_region.dstOffset = 0;
// 	copy_region.size = dst_buffer.size;
// 	vkCmdCopyBuffer(cmdbuf, handle, dst_buffer.handle, 1, &copy_region);
// 	VkBufferMemoryBarrier copy_barrier = vk::buffer_barrier(dst_buffer.handle, VK_ACCESS_TRANSFER_WRITE_BIT,
// 															VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
// 	vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
// 						 VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 1, &copy_barrier, 0, 0);
// }

// void BufferOld::destroy() {
// 	if (handle) vkDestroyBuffer(vk::context().device, handle, nullptr);
// 	if (buffer_memory) vkFreeMemory(vk::context().device, buffer_memory, nullptr);
// }

// void BufferOld::bind(VkDeviceSize offset /*=0*/) {
// 	vk::check(vkBindBufferMemory(vk::context().device, handle, buffer_memory, offset));
// }

// void BufferOld::map(VkDeviceSize size /*= VK_WHOLE_SIZE*/, VkDeviceSize offset /*= 0*/) {
// 	vk::check(vkMapMemory(vk::context().device, buffer_memory, offset, size, 0, &data), "Unable to map memory");
// }

// void BufferOld::unmap() { vkUnmapMemory(vk::context().device, buffer_memory); }

// VkDeviceAddress BufferOld::get_device_address() {
// 	VkBufferDeviceAddressInfo info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
// 	info.buffer = handle;
// 	return vkGetBufferDeviceAddress(vk::context().device, &info);
// }

// void BufferOld::create(VkBufferUsageFlags flags, VkMemoryPropertyFlags mem_property_flags, VkDeviceSize size,
// 					   void* data, bool use_staging, VkSharingMode sharing_mode) {
// 	return create("", flags, mem_property_flags, size, data, use_staging, sharing_mode);
// }

// }  // namespace lumen

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
	VkBufferCreateInfo buffer_ci = vk::buffer(buffer->usage_flags, buffer->size, VK_SHARING_MODE_EXCLUSIVE);
	VmaAllocationInfo alloc_info;
	vmaCreateBuffer(vk::context().allocator, &buffer_ci, &alloc_ci, &buffer->handle, &buffer->allocation, &alloc_info);
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
										   .data = desc.data});
		lumen::CommandBuffer cmd(true);
		VkBufferCopy copy_region = {
			.size = buffer->size,
		};
		vkCmdCopyBuffer(cmd.handle, staging_buffer->handle, buffer->handle, 1, &copy_region);
		cmd.submit(vk::context().queues[0]);
		drm::destroy(staging_buffer);

	} else if (desc.data) {
		memcpy(alloc_info.pMappedData, desc.data, buffer->size);
		vmaFlushAllocation(vk::context().allocator, buffer->allocation, 0, buffer->size);
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

void* read_buffer(Buffer* buffer) {
	VkMemoryPropertyFlags mem_prop_flags;
	vmaGetAllocationMemoryProperties(vk::context().allocator, buffer->allocation, &mem_prop_flags);
	LUMEN_ASSERT((mem_prop_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0, "Buffer is not host visible");
	void* memory;
	vmaMapMemory(vk::context().allocator, buffer->allocation, &memory);
	return memory;
}

}  // namespace vk