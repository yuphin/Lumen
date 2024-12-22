#include "../LumenPCH.h"
#include "Buffer.h"
#pragma once

namespace vk {
class SBTWrapper {
   public:
	enum GroupType { GROUP_RAYGEN, GROUP_MISS, GROUP_HIT, GROUP_CALLABLE };

	void setup(uint32_t family_idx, const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rt_props);
	void destroy();
	void create(VkPipeline rtPipeline, VkRayTracingPipelineCreateInfoKHR pipeline_info = {});

	void add_indices(VkRayTracingPipelineCreateInfoKHR pipeline_info);

	void add_index(GroupType t, uint32_t index) { idx_array[t].push_back(index); }

	template <typename T>
	void add_data(GroupType t, uint32_t groupIndex, T& data) {
		add_data(t, groupIndex, (uint8_t*)&data, sizeof(T));
	}

	void add_data(GroupType t, uint32_t group_idx, uint8_t* data, size_t data_size) {
		std::vector<uint8_t> dst(data, data + data_size);
		group_data[t].handle_alignment[group_idx] = dst;
	}

	uint32_t index_count(GroupType t) { return static_cast<uint32_t>(idx_array[t].size()); }
	uint32_t get_stride(GroupType t) { return group_data[t].stride; }
	uint32_t get_size(GroupType t) { return get_stride(t) * index_count(t); }
	VkDeviceAddress get_address(GroupType t);
	const VkStridedDeviceAddressRegionKHR get_region(GroupType t);
	const std::array<VkStridedDeviceAddressRegionKHR, 4> get_regions();

   private:
	using Entry = std::unordered_map<uint32_t, std::vector<uint8_t>>;
	struct GroupData {
		uint32_t stride = 0;
		vk::Buffer* buffer = nullptr;
		Entry handle_alignment = {};
	};

	std::array<GroupData, 4> group_data;
	std::array<std::vector<uint32_t>, 4> idx_array;
	uint32_t handle_size{0};
	uint32_t handle_alignment{0};
	uint32_t queue_idx{0};
};

}  // namespace vk
