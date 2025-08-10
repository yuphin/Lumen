#include "Buffer.h"
#pragma once

namespace vk {
class SBTWrapper {
   public:
	enum GroupType { GROUP_RAYGEN, GROUP_MISS, GROUP_HIT, GROUP_CALLABLE };

	void setup(u32 family_idx, const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rt_props);
	void destroy();
	void create(VkPipeline rtPipeline, VkRayTracingPipelineCreateInfoKHR pipeline_info = {});

	void add_indices(VkRayTracingPipelineCreateInfoKHR pipeline_info);

	void add_index(GroupType t, u32 index) { idx_array[t].push_back(index); }

	template <typename T>
	void add_data(GroupType t, u32 groupIndex, T& data) {
		add_data(t, groupIndex, (u8*)&data, sizeof(T));
	}

	void add_data(GroupType t, u32 group_idx, u8* data, u64 data_size) {
		std::vector<u8> dst(data, data + data_size);
		group_data[t].handle_alignment[group_idx] = dst;
	}

	u32 index_count(GroupType t) { return static_cast<u32>(idx_array[t].size()); }
	u32 get_stride(GroupType t) { return group_data[t].stride; }
	u32 get_size(GroupType t) { return get_stride(t) * index_count(t); }
	VkDeviceAddress get_address(GroupType t);
	const VkStridedDeviceAddressRegionKHR get_region(GroupType t);
	const std::array<VkStridedDeviceAddressRegionKHR, 4> get_regions();

   private:
	using Entry = std::unordered_map<u32, std::vector<u8>>;
	struct GroupData {
		u32 stride = 0;
		vk::Buffer* buffer = nullptr;
		Entry handle_alignment = {};
	};

	std::array<GroupData, 4> group_data;
	std::array<std::vector<u32>, 4> idx_array;
	u32 handle_size{0};
	u32 handle_alignment{0};
	u32 queue_idx{0};
};

}  // namespace vk
