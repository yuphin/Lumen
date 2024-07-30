

#pragma once
#include "../LumenPCH.h"
#include "Framework/Buffer.h"
namespace lumen {
struct AccelKHR {
	VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
	Buffer buffer;

	VkDeviceAddress get_blas_device_address() const {
		VkAccelerationStructureDeviceAddressInfoKHR addr_info{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
		addr_info.accelerationStructure = accel;
		return vkGetAccelerationStructureDeviceAddressKHR(vk::context().device, &addr_info);
	}
};
}  // namespace
