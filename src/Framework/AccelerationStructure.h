

#pragma once
#include "../LumenPCH.h"
#include "Buffer.h"
namespace vk {

struct BVH {
	VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
	lumen::Buffer buffer;

	VkDeviceAddress get_blas_device_address() const {
		VkAccelerationStructureDeviceAddressInfoKHR addr_info{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
		addr_info.accelerationStructure = accel;
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
				VkBuildAccelerationStructureFlagsKHR flags);
void build_tlas(BVH& tlas, std::vector<VkAccelerationStructureInstanceKHR>& instances,
				VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
				bool update = false);
}  // namespace vk
