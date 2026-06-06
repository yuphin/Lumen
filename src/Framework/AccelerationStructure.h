

#pragma once
#include "Buffer.h"
#include "Framework/Base/Memory.h"
namespace vk {

struct BVH {
	VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
	vk::Buffer* buffer = nullptr;
	inline VkDeviceAddress device_address() const {
		VkAccelerationStructureDeviceAddressInfoKHR addr_info{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, .accelerationStructure = accel};
		return vkGetAccelerationStructureDeviceAddressKHR(vk::context().device, &addr_info);
	}

	void destroy();
};

struct BlasInput {
	VkAccelerationStructureGeometryKHR geometry;
	VkAccelerationStructureBuildRangeInfoKHR build_range;
	VkBuildAccelerationStructureFlagsKHR flags = 0;
};
BlasInput blas_input_create(u32 vtx_count, u32 idx_count, u32 vtx_offset, u32 first_idx, VkDeviceAddress vertex_address,
							u64 vertex_stride, VkDeviceAddress index_address);
void blas_build(lm::ScratchArena& scratch, util::Slice<BVH> blases, util::Slice<BlasInput> input,
				VkBuildAccelerationStructureFlagsKHR flags, VkCommandBuffer cmd = VK_NULL_HANDLE,
				vk::Buffer** scratch_buffer_ref = nullptr);
void tlas_build(BVH& tlas, util::Slice<VkAccelerationStructureInstanceKHR> instances,
				VkBuildAccelerationStructureFlagsKHR flags, bool update = false);
void tlas_build(BVH& tlas, vk::Buffer* instances_buf, u32 instance_count, VkBuildAccelerationStructureFlagsKHR flags,
				VkCommandBuffer cmd_buf, vk::Buffer** scratch_buffer_ref, bool update = false);

}  // namespace vk
