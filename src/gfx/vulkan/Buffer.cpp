#include "lmhpch.h"
#include "Buffer.h"
void Buffer::flush(VkDeviceSize size, VkDeviceSize offset) {
	VkMappedMemoryRange mapped_range = {};
	mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mapped_range.memory = buffer_memory;
	mapped_range.offset = offset;
	mapped_range.size = size;
	check(vkFlushMappedMemoryRanges(device, 1, &mapped_range), "Failed to flush mapped memory ranges");
}

void Buffer::invalidate(VkDeviceSize size, VkDeviceSize offset) {
	VkMappedMemoryRange mapped_range = {};
	mapped_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mapped_range.memory = buffer_memory;
	mapped_range.offset = offset;
	mapped_range.size = size;
	check(vkInvalidateMappedMemoryRanges(device, 1, &mapped_range),
		"Failed to invalidate mapped memory range");
}

void Buffer::prepare_descriptor(VkDeviceSize size, VkDeviceSize offset) {
	descriptor.offset = offset;
	descriptor.buffer = handle;
	descriptor.range = size;
}
