#include "SBTWrapper.h"
#include "VkUtils.h"
#include "CommandBuffer.h"
#include "PersistentResourceManager.h"
#include "Framework/Base/Utils.h"

namespace vk {

void SBTWrapper::destroy() {
	for (auto& group : group_data) {
		prm::remove(group.buffer);
	}
	for (auto& shaders : idx_array) {
		shaders.clear();
	}
}

void SBTWrapper::add_indices(VkRayTracingPipelineCreateInfoKHR info) {
	for (auto& shaders : idx_array) {
		shaders.clear();
	};
	u32 stage_idx = 0;
	for (u32 group_idx = 0; group_idx < info.groupCount; group_idx++) {
		LUMEN_ASSERT(group_idx == stage_idx, "Currently 1 stage = 1 group");
		if (info.pGroups[group_idx].type == VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR) {
			if (info.pStages[stage_idx].stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR) {
				idx_array[GROUP_RAYGEN].push_back(group_idx);
				stage_idx++;
			} else if (info.pStages[stage_idx].stage == VK_SHADER_STAGE_MISS_BIT_KHR) {
				idx_array[GROUP_MISS].push_back(group_idx);
				stage_idx++;
			} else if (info.pStages[stage_idx].stage == VK_SHADER_STAGE_CALLABLE_BIT_KHR) {
				idx_array[GROUP_CALLABLE].push_back(group_idx);
				stage_idx++;
			}
		} else {
			// mainly for any hit and closest hit shaders
			idx_array[GROUP_HIT].push_back(group_idx);
			if (info.pGroups[group_idx].closestHitShader != VK_SHADER_UNUSED_KHR) stage_idx++;
			if (info.pGroups[group_idx].anyHitShader != VK_SHADER_UNUSED_KHR) stage_idx++;
			// if (info.pGroups[group_idx].intersectionShader != VK_SHADER_UNUSED_KHR) stage_idx++;
		}
	}
}
void SBTWrapper::create(VkPipeline rt_pipeline, VkRayTracingPipelineCreateInfoKHR pipeline_info /*= {}*/) {
	assert(pipeline_info.sType == VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR);
	u32 group_handle_size = vk::context().rt_props.shaderGroupHandleSize;
	u32 group_handle_alignment = vk::context().rt_props.shaderGroupHandleAlignment;
	u32 group_stride = util::align_up_pow2(group_handle_size, group_handle_alignment);
	for (GroupData& group : group_data) {
		prm::remove(group.buffer);
		group.stride = group_stride;
	}

	add_indices(pipeline_info);
	u32 sbt_size = pipeline_info.groupCount * group_handle_size;

	constexpr u64 MAX_HANDLE_SIZE = 32;
	constexpr u64 MAX_SBT_SIZE_BYTES = vk::MAX_SHADERS_PER_PASS * MAX_HANDLE_SIZE;
	assert(group_handle_size <= MAX_HANDLE_SIZE);
	assert(sbt_size < MAX_SBT_SIZE_BYTES);
	lm::SmallArray<u8, MAX_SBT_SIZE_BYTES> shader_handle_storage;

	vk::check(vkGetRayTracingShaderGroupHandlesKHR(vk::context().device, rt_pipeline, 0, pipeline_info.groupCount,
												   sbt_size, shader_handle_storage.data));


	constexpr u64 MAX_BYTES_PER_GROUP = MAX_SBT_SIZE_BYTES / 4;
	lm::SmallArray<lm::SmallArray<u8, MAX_BYTES_PER_GROUP>, 4> stage;
	stage.resize(4);

	auto copy_handles = [&](util::Slice<u8> stage_buffer, util::Slice<u32> indices, u32 stride) {
		auto* pbuffer = stage_buffer.data;
		for (u64 index = 0; index < indices.size; index++) {
			auto* pstart = pbuffer;
			memcpy(pbuffer, shader_handle_storage.data + (indices[index] * group_handle_size), group_handle_size);
			pbuffer = pstart + stride;
		}
	};
	stage[GROUP_RAYGEN].size = group_data[GROUP_RAYGEN].stride * index_count(GROUP_RAYGEN);
	stage[GROUP_MISS].size = group_data[GROUP_MISS].stride * index_count(GROUP_MISS);
	stage[GROUP_HIT].size = group_data[GROUP_HIT].stride * index_count(GROUP_HIT);
	stage[GROUP_CALLABLE].size = group_data[GROUP_CALLABLE].stride * index_count(GROUP_CALLABLE);
	assert((stage[GROUP_RAYGEN].size + stage[GROUP_MISS].size + stage[GROUP_HIT].size + stage[GROUP_CALLABLE].size) <= MAX_SBT_SIZE_BYTES);

	copy_handles(stage[GROUP_RAYGEN].to_slice(), idx_array[GROUP_RAYGEN].to_slice(), group_data[GROUP_RAYGEN].stride);
	copy_handles(stage[GROUP_MISS].to_slice(), idx_array[GROUP_MISS].to_slice(), group_data[GROUP_MISS].stride);
	copy_handles(stage[GROUP_HIT].to_slice(), idx_array[GROUP_HIT].to_slice(), group_data[GROUP_HIT].stride);
	copy_handles(stage[GROUP_CALLABLE].to_slice(), idx_array[GROUP_CALLABLE].to_slice(),
				 group_data[GROUP_CALLABLE].stride);

	VkBufferUsageFlags usage_flags =
		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
	for (u32 i = 0; i < 4; i++) {
		if (!stage[i].empty()) {
			// Can be called from multiple threads
			group_data[i].buffer = prm::get_buffer({.name = CSTR("SBT Buffer"),
													.usage = usage_flags,
													.memory_type = vk::BUFFER_TYPE_GPU,
													.size = stage[i].size,
													.data = stage[i].data},
												   /*use_mutex=*/true);
		}
	}
}

VkDeviceAddress SBTWrapper::get_address(GroupType t) {
	if (!group_data[t].buffer || !group_data[t].buffer->size) {
		return 0;
	}
	return group_data[t].buffer->device_address();
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
