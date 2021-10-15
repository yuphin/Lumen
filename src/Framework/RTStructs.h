#pragma once
#include "Buffer.h"

struct AccelKHR {
	VkAccelerationStructureKHR accel = VK_NULL_HANDLE;
	Buffer buffer;
};

struct BuildAccelerationStructure
{
	VkAccelerationStructureBuildGeometryInfoKHR build_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	VkAccelerationStructureBuildSizesInfoKHR size_info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	const VkAccelerationStructureBuildRangeInfoKHR* range_info;
	AccelKHR as;  // result acceleration structure
	AccelKHR cleanup_as;
};

class RTAccels {
	std::vector<AccelKHR> blases;
	AccelKHR tlas;
};
