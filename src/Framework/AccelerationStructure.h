

#pragma once
#include "../LumenPCH.h"
#include "Buffer.h"
namespace vk {

struct BVH {
	VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
	vk::Buffer* buffer;
	// This is usually set to false
	// Except when the BVH was generated within the render graph
	bool needs_sync = false;

	inline VkDeviceAddress get_device_address() const {
		VkAccelerationStructureDeviceAddressInfoKHR addr_info{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, .accelerationStructure = accel};
		return vkGetAccelerationStructureDeviceAddressKHR(vk::context().device, &addr_info);
	}
};

struct BlasInput {
	// Data used to build acceleration structure geometry
	std::vector<VkAccelerationStructureGeometryKHR> as_geom;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> as_build_offset_info;
	VkBuildAccelerationStructureFlagsKHR flags{0};
};
void build_blas(std::vector<BVH>& blases, const std::vector<BlasInput>& input,
				VkBuildAccelerationStructureFlagsKHR flags, VkCommandBuffer = VK_NULL_HANDLE, vk::Buffer** scratch_buffer = nullptr);
void build_tlas(BVH& tlas, std::vector<VkAccelerationStructureInstanceKHR>& instances,
				VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
				bool update = false);
}  // namespace vk
