#include "Buffer.h"
#include <array>
#pragma once

namespace vk {
class SBTWrapper {
   public:
	enum GroupType { GROUP_RAYGEN, GROUP_MISS, GROUP_HIT, GROUP_CALLABLE };

	void destroy();
	void create(VkPipeline rtPipeline, VkRayTracingPipelineCreateInfoKHR pipeline_info = {});

	void add_indices(VkRayTracingPipelineCreateInfoKHR pipeline_info);

	void add_index(GroupType t, u32 index) { idx_array[t].push_back(index); }

	u32 index_count(GroupType t) { return static_cast<u32>(idx_array[t].size()); }
	u32 get_stride(GroupType t) { return group_data[t].stride; }
	u32 get_size(GroupType t) { return get_stride(t) * index_count(t); }
	VkDeviceAddress get_address(GroupType t);
	const VkStridedDeviceAddressRegionKHR get_region(GroupType t);
	const std::array<VkStridedDeviceAddressRegionKHR, 4> get_regions();

   private:
	struct GroupData {
		u32 stride = 0;
		vk::Buffer* buffer = nullptr;
	};

	// TODO: Add SmallArray here
	std::array<GroupData, 4> group_data = {};
	std::array<std::vector<u32>, 4> idx_array = {};
};

}  // namespace vk
