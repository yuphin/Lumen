#pragma once
#include "Buffer.h"
#include "Framework/Base/SmallArray.h"

namespace vk {

inline constexpr u64 MAX_RT_SHADER_PER_GROUP = 8;
inline constexpr u64 NUM_SBT_GROUPS = 4;

class SBTWrapper {
   public:
	enum GroupType { GROUP_RAYGEN, GROUP_MISS, GROUP_HIT, GROUP_CALLABLE };

	void destroy();
	void create(VkPipeline rtPipeline, VkRayTracingPipelineCreateInfoKHR pipeline_info = {});

	void add_indices(VkRayTracingPipelineCreateInfoKHR pipeline_info);

	void add_index(GroupType t, u32 index) { idx_array[t].push_back(index); }

	u32 index_count(GroupType t) { return static_cast<u32>(idx_array[t].size); }
	u32 get_stride(GroupType t) { return group_data[t].stride; }
	u32 get_size(GroupType t) { return get_stride(t) * index_count(t); }
	VkDeviceAddress get_address(GroupType t);
	VkStridedDeviceAddressRegionKHR get_region(GroupType t);
	lm::SmallArray<VkStridedDeviceAddressRegionKHR, NUM_SBT_GROUPS> get_regions();

   private:
	struct GroupData {
		u32 stride = 0;
		vk::Buffer* buffer = nullptr;
	};

	GroupData group_data[NUM_SBT_GROUPS] = {};
	lm::SmallArray<u32, MAX_RT_SHADER_PER_GROUP> idx_array[NUM_SBT_GROUPS];
};

}  // namespace vk
