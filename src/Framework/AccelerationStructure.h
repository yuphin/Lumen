

#pragma once
#include "../LumenPCH.h"
#include "Buffer.h"
namespace vk {

struct BVH {
	VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
	vk::Buffer* buffer = nullptr;
	inline VkDeviceAddress get_device_address() const {
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
void build_blas(std::vector<BVH>& blases, const std::vector<BlasInput>& input,
				VkBuildAccelerationStructureFlagsKHR flags, VkCommandBuffer cmd = VK_NULL_HANDLE,
				vk::Buffer** scratch_buffer_ref = nullptr);
void build_blas(util::Slice<BVH> blases, const std::vector<BlasInput>& input,
				VkBuildAccelerationStructureFlagsKHR flags, VkCommandBuffer cmd = VK_NULL_HANDLE,
				vk::Buffer** scratch_buffer_ref = nullptr);
void build_tlas(BVH& tlas, std::vector<VkAccelerationStructureInstanceKHR>& instances,
				VkBuildAccelerationStructureFlagsKHR flags, bool update = false);
void build_tlas(BVH& tlas, vk::Buffer* instances_buf, uint32_t instance_count,
				VkBuildAccelerationStructureFlagsKHR flags, VkCommandBuffer cmd_buf, vk::Buffer** scratch_buffer_ref,
				bool update = false);

}  // namespace vk
