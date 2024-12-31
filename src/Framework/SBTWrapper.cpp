#include "../LumenPCH.h"
#include "SBTWrapper.h"
#include "VkUtils.h"
#include "CommandBuffer.h"
#include "PersistentResourceManager.h"

namespace vk {

template <class T>
static constexpr T align_up(T x, size_t a) noexcept {
	return T((x + (T(a) - 1)) & ~T(a - 1));
}

void SBTWrapper::setup(uint32_t family_idx, const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rt_props) {
	queue_idx = family_idx;
	handle_size = rt_props.shaderGroupHandleSize;
	handle_alignment = rt_props.shaderGroupHandleAlignment;
}

void SBTWrapper::destroy() {
	for (auto& group : group_data) {
		prm::remove(group.buffer);
	}
	for (auto& i : idx_array) i = {};
}

void SBTWrapper::add_indices(VkRayTracingPipelineCreateInfoKHR info) {
	for (auto& i : idx_array) {
		i = {};
	};
	uint32_t stage_idx = 0;
	uint32_t group_offset = stage_idx;
	for (uint32_t g = 0; g < info.groupCount; g++) {
		if (info.pGroups[g].type == VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR) {
			if (info.pStages[stage_idx].stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR) {
				idx_array[GROUP_RAYGEN].push_back(g + group_offset);
				stage_idx++;
			} else if (info.pStages[stage_idx].stage == VK_SHADER_STAGE_MISS_BIT_KHR) {
				idx_array[GROUP_MISS].push_back(g + group_offset);
				stage_idx++;
			} else if (info.pStages[stage_idx].stage == VK_SHADER_STAGE_CALLABLE_BIT_KHR) {
				idx_array[GROUP_CALLABLE].push_back(g + group_offset);
				stage_idx++;
			}
		} else {
			idx_array[GROUP_HIT].push_back(g + group_offset);
			if (info.pGroups[g].closestHitShader != VK_SHADER_UNUSED_KHR) stage_idx++;
			if (info.pGroups[g].anyHitShader != VK_SHADER_UNUSED_KHR) stage_idx++;
			if (info.pGroups[g].intersectionShader != VK_SHADER_UNUSED_KHR) stage_idx++;
		}
	}
}
void SBTWrapper::create(VkPipeline rt_pipeline, VkRayTracingPipelineCreateInfoKHR pipeline_info /*= {}*/) {
	for (GroupData& group : group_data) {
		prm::remove(group.buffer);
	}

	uint32_t total_group_cnt{0};
	std::vector<uint32_t> group_cnt_per_input;
	if (pipeline_info.sType == VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR) {
		add_indices(pipeline_info);
		group_cnt_per_input.push_back(pipeline_info.groupCount);
		total_group_cnt += pipeline_info.groupCount;
	} else {
		for (auto& i : idx_array) {
			if (!i.empty()) total_group_cnt = std::max(total_group_cnt, *std::max_element(std::begin(i), std::end(i)));
		}
		total_group_cnt++;
		group_cnt_per_input.push_back(total_group_cnt);
	}
	uint32_t sbt_size = total_group_cnt * handle_size;
	std::vector<uint8_t> shader_handle_storage(sbt_size);

	vk::check(vkGetRayTracingShaderGroupHandlesKHR(vk::context().device, rt_pipeline, 0, total_group_cnt, sbt_size,
												   shader_handle_storage.data()));
	auto find_stride = [&](const Entry& entry, uint32_t& stride) {
		stride = align_up(handle_size, handle_alignment);  // minimum stride
		for (auto& e : entry) {
			uint32_t data_handle_size =
				align_up(static_cast<uint32_t>(handle_size + e.second.size() * sizeof(uint8_t)), handle_alignment);
			stride = std::max(stride, data_handle_size);
		}
	};
	find_stride(group_data[GROUP_RAYGEN].handle_alignment, group_data[GROUP_RAYGEN].stride);
	find_stride(group_data[GROUP_MISS].handle_alignment, group_data[GROUP_MISS].stride);
	find_stride(group_data[GROUP_HIT].handle_alignment, group_data[GROUP_HIT].stride);
	find_stride(group_data[GROUP_CALLABLE].handle_alignment, group_data[GROUP_CALLABLE].stride);

	std::array<std::vector<uint8_t>, 4> stage;
	stage[GROUP_RAYGEN] = std::vector<uint8_t>(group_data[GROUP_RAYGEN].stride * index_count(GROUP_RAYGEN));
	stage[GROUP_MISS] = std::vector<uint8_t>(group_data[GROUP_MISS].stride * index_count(GROUP_MISS));
	stage[GROUP_HIT] = std::vector<uint8_t>(group_data[GROUP_HIT].stride * index_count(GROUP_HIT));
	stage[GROUP_CALLABLE] = std::vector<uint8_t>(group_data[GROUP_CALLABLE].stride * index_count(GROUP_CALLABLE));

	auto copy_handles = [&](std::vector<uint8_t>& stage_buffer, std::vector<uint32_t>& indices, uint32_t stride,
							auto& data) {
		auto* pbuffer = stage_buffer.data();
		for (uint32_t index = 0; index < static_cast<uint32_t>(indices.size()); index++) {
			auto* pstart = pbuffer;
			// Copy the handle
			memcpy(pbuffer, shader_handle_storage.data() + (indices[index] * handle_size), handle_size);
			// If there is data for this group index, copy it too
			auto it = data.find(index);
			if (it != std::end(data)) {
				pbuffer += handle_size;
				memcpy(pbuffer, it->second.data(), it->second.size() * sizeof(uint8_t));
			}
			pbuffer = pstart + stride;	// Jumping to next group
		}
	};

	copy_handles(stage[GROUP_RAYGEN], idx_array[GROUP_RAYGEN], group_data[GROUP_RAYGEN].stride,
				 group_data[GROUP_RAYGEN].handle_alignment);
	copy_handles(stage[GROUP_MISS], idx_array[GROUP_MISS], group_data[GROUP_MISS].stride,
				 group_data[GROUP_MISS].handle_alignment);
	copy_handles(stage[GROUP_HIT], idx_array[GROUP_HIT], group_data[GROUP_HIT].stride,
				 group_data[GROUP_HIT].handle_alignment);
	copy_handles(stage[GROUP_CALLABLE], idx_array[GROUP_CALLABLE], group_data[GROUP_CALLABLE].stride,
				 group_data[GROUP_CALLABLE].handle_alignment);

	VkBufferUsageFlags usage_flags =
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
	for (uint32_t i = 0; i < 4; i++) {
		if (!stage[i].empty()) {
			// Can be called from multiple threads
			group_data[i].buffer = prm::get_buffer({.name = "SBT " + std::to_string(i),
													.usage = usage_flags,
													.memory_type = vk::BufferType::GPU,
													.size = stage[i].size(),
													.data = stage[i].data()},
												   /*use_mutex=*/true);
		}
	}
}

VkDeviceAddress SBTWrapper::get_address(GroupType t) {
	if (!group_data[t].buffer || !group_data[t].buffer->size) {
		return 0;
	}
	return group_data[t].buffer->get_device_address();
}

const VkStridedDeviceAddressRegionKHR SBTWrapper::get_region(GroupType t) {
	return VkStridedDeviceAddressRegionKHR{get_address(t), get_stride(t), get_size(t)};
}

const std::array<VkStridedDeviceAddressRegionKHR, 4> SBTWrapper::get_regions() {
	std::array<VkStridedDeviceAddressRegionKHR, 4> regions{get_region(GROUP_RAYGEN), get_region(GROUP_MISS),
														   get_region(GROUP_HIT), get_region(GROUP_CALLABLE)};
	return regions;
}

}  // namespace vk
