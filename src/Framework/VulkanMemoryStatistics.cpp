#include "VulkanMemoryStatistics.h"

#include "Base/OS.h"
#include "VulkanContext.h"

namespace vk {

static GpuAllocationRecord* _allocation_head = nullptr;
static os::Mutex _allocation_mutex;
static u64 _next_allocation_id = 1;

void gpu_allocation_register(GpuAllocationRecord* record, VmaAllocation allocation, GpuAllocationKind kind,
							 lm::String name) {
	LUMEN_ASSERT(record && allocation, "Cannot register an empty GPU allocation");
	LUMEN_ASSERT(name.empty() || name.is_cstr(), "GPU allocation name must be a C string");

	const char* allocation_name =
		name.empty() ? (kind == GPU_ALLOCATION_BUFFER ? "Unnamed Buffer" : "Unnamed Image") : name.data;
	vmaSetAllocationName(vk::context().allocator, allocation, allocation_name);
	VmaAllocationInfo allocation_info = {};
	vmaGetAllocationInfo(vk::context().allocator, allocation, &allocation_info);

	os::ScopedLock lock(_allocation_mutex);
	LUMEN_ASSERT(record->allocation == VK_NULL_HANDLE, "GPU allocation is already registered");
	record->allocation = allocation;
	record->kind = kind;
	record->size = allocation_info.size;
	record->id = _next_allocation_id++;

	GpuAllocationRecord* previous = nullptr;
	GpuAllocationRecord* insertion_point = _allocation_head;
	// Move insertion point to the first record with a smaller size than the new record
	while (insertion_point && insertion_point->size >= record->size) {
		previous = insertion_point;
		insertion_point = insertion_point->next;
	}
	// Note: insertion_point == nullptr if appending at the tail

	record->prev = previous;
	record->next = insertion_point;
	if (previous) {
		previous->next = record;
	} else {
		// No previous -> this is the head
		_allocation_head = record;
	}
	if (insertion_point) {
		insertion_point->prev = record;
	}
}

void gpu_allocation_unregister(GpuAllocationRecord* record) {
	if (!record || record->allocation == VK_NULL_HANDLE) return;

	os::ScopedLock lock(_allocation_mutex);
	if (record->prev) {
		record->prev->next = record->next;
	} else {
		LUMEN_ASSERT(_allocation_head == record, "GPU allocation registry is corrupted");
		_allocation_head = record->next;
	}
	if (record->next) {
		record->next->prev = record->prev;
	}
	*record = {};
}

GpuMemorySummary get_gpu_memory_stats(GpuAllocationKind kind, GpuAllocationStatsCallback callback) {
	GpuMemorySummary result = {};
	VmaBudget budgets[VK_MAX_MEMORY_HEAPS] = {};
	vmaGetHeapBudgets(vk::context().allocator, budgets);

	const VkPhysicalDeviceMemoryProperties& memory_properties = vk::context().memory_properties;
	for (u32 heap_idx = 0; heap_idx < memory_properties.memoryHeapCount; ++heap_idx) {
		const VmaBudget& budget = budgets[heap_idx];
		result.allocation_bytes += budget.statistics.allocationBytes;
		result.block_bytes += budget.statistics.blockBytes;
		result.allocation_count += budget.statistics.allocationCount;
		result.block_count += budget.statistics.blockCount;
		if (memory_properties.memoryHeaps[heap_idx].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
			result.driver_usage_bytes += budget.usage;
			result.driver_budget_bytes += budget.budget;
		}
	}

	os::ScopedLock lock(_allocation_mutex);
	for (GpuAllocationRecord* record = _allocation_head; record; record = record->next) {
		VmaAllocationInfo allocation_info = {};
		vmaGetAllocationInfo(vk::context().allocator, record->allocation, &allocation_info);
		LUMEN_ASSERT(allocation_info.memoryType < memory_properties.memoryTypeCount,
					 "VMA returned an invalid memory type");

		const VkMemoryType& memory_type = memory_properties.memoryTypes[allocation_info.memoryType];
		GpuAllocationStats stats = {
			.name = allocation_info.pName
						? allocation_info.pName
						: (record->kind == GPU_ALLOCATION_BUFFER ? "Unnamed Buffer" : "Unnamed Image"),
			.id = record->id,
			.size = allocation_info.size,
			.memory_type_index = allocation_info.memoryType,
			.heap_index = memory_type.heapIndex,
			.memory_properties = memory_type.propertyFlags,
			.kind = record->kind,
		};

		if (record->kind == GPU_ALLOCATION_BUFFER) {
			result.tracked_buffer_bytes += stats.size;
			++result.tracked_buffer_count;
		} else {
			result.tracked_image_bytes += stats.size;
			++result.tracked_image_count;
		}
		if (callback && (kind == GPU_ALLOCATION_ALL || stats.kind == kind)) {
			callback(stats);
		}
	}

	const VkDeviceSize tracked_bytes = result.tracked_buffer_bytes + result.tracked_image_bytes;
	const u32 tracked_count = result.tracked_buffer_count + result.tracked_image_count;
	result.untracked_bytes = result.allocation_bytes > tracked_bytes ? result.allocation_bytes - tracked_bytes : 0;
	result.untracked_count = result.allocation_count > tracked_count ? result.allocation_count - tracked_count : 0;
	return result;
}

}  // namespace vk
