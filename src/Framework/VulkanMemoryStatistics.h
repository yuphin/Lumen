#pragma once

#include "Base/String.h"

namespace vk {

enum GpuAllocationKind : u32 {
	GPU_ALLOCATION_BUFFER,
	GPU_ALLOCATION_IMAGE,
	GPU_ALLOCATION_ALL,
};

struct GpuAllocationRecord {
	GpuAllocationRecord* prev = nullptr;
	GpuAllocationRecord* next = nullptr;
	VmaAllocation allocation = VK_NULL_HANDLE;
	GpuAllocationKind kind = GPU_ALLOCATION_BUFFER;
	VkDeviceSize size = 0;
	u64 id = 0;
};

struct GpuAllocationStats {
	const char* name = nullptr;
	u64 id = 0;
	VkDeviceSize size = 0;
	u32 memory_type_index = 0;
	u32 heap_index = 0;
	VkMemoryPropertyFlags memory_properties = 0;
	GpuAllocationKind kind = GPU_ALLOCATION_BUFFER;
};

struct GpuMemorySummary {
	VkDeviceSize allocation_bytes = 0;
	VkDeviceSize block_bytes = 0;
	VkDeviceSize driver_usage_bytes = 0;
	VkDeviceSize driver_budget_bytes = 0;
	VkDeviceSize tracked_buffer_bytes = 0;
	VkDeviceSize tracked_image_bytes = 0;
	VkDeviceSize untracked_bytes = 0;
	u32 allocation_count = 0;
	u32 block_count = 0;
	u32 tracked_buffer_count = 0;
	u32 tracked_image_count = 0;
	u32 untracked_count = 0;
};

using GpuAllocationStatsCallback = void (*)(const GpuAllocationStats& stats);

void gpu_allocation_register(GpuAllocationRecord* record, VmaAllocation allocation, GpuAllocationKind kind,
							 lm::String name);
void gpu_allocation_unregister(GpuAllocationRecord* record);
GpuMemorySummary get_gpu_memory_stats(GpuAllocationKind kind = GPU_ALLOCATION_ALL,
									 GpuAllocationStatsCallback callback = nullptr);

}  // namespace vk
