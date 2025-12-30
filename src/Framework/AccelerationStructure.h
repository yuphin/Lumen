

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
	// Data used to build acceleration structure geometry
	std::vector<VkAccelerationStructureGeometryKHR> as_geom;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> as_build_offset_info;
	VkBuildAccelerationStructureFlagsKHR flags{0};
};
void blas_build(lm::ScratchArena& scratch, lm::Array<BVH>& blases, lm::FixedArray<BlasInput>& inputs,
				VkBuildAccelerationStructureFlagsKHR flags, VkCommandBuffer cmd = VK_NULL_HANDLE,
				vk::Buffer** scratch_buffer_ref = nullptr);
void blas_build(lm::ScratchArena& scratch, util::Slice<BVH> blases, util::Slice<BlasInput> input,
				VkBuildAccelerationStructureFlagsKHR flags, VkCommandBuffer cmd = VK_NULL_HANDLE,
				vk::Buffer** scratch_buffer_ref = nullptr);
void tlas_build(BVH& tlas, std::vector<VkAccelerationStructureInstanceKHR>& instances,
				VkBuildAccelerationStructureFlagsKHR flags, bool update = false);
void tlas_build(BVH& tlas, vk::Buffer* instances_buf, u32 instance_count, VkBuildAccelerationStructureFlagsKHR flags,
				VkCommandBuffer cmd_buf, vk::Buffer** scratch_buffer_ref, bool update = false);

BlasInput to_vk_geometry(u32 vtx_count, u32 idx_count, u32 vtx_offset, u32 first_idx, VkDeviceAddress vertex_address,
						 VkDeviceAddress index_address);
}  // namespace vk
