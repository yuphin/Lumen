#include "LumenPCH.h"
#include "SBTWrapper.h"
#include "Framework/Utils.h"
#include "Framework/CommandBuffer.h"


//--------------------------------------------------------------------------------------------------
// Default setup
//
void SBTWrapper::setup(VulkanContext* ctx,
					   uint32_t family_idx,
					   const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rt_props) {
	m_ctx = ctx;
	m_queue_idx = family_idx;
	m_handle_size = rt_props.shaderGroupHandleSize;       // Size of a program identifier
	m_handle_alignment = rt_props.shaderGroupHandleAlignment;  // Alignment in bytes for each SBT entry
}

//--------------------------------------------------------------------------------------------------
// Destroying the allocated buffers and clearing all vectors
//
void SBTWrapper::destroy() {
	for (auto& b : m_buffer) {
		int a = 4;
		b.destroy();
	}
	for (auto& i : m_index)
		i = {};
}

//--------------------------------------------------------------------------------------------------
// Finding the handle index position of each group type in the pipeline creation info.
// If the pipeline was created like: raygen, miss, hit, miss, hit, hit
// The result will be: raygen[0], miss[1, 3], hit[2, 4, 5], callable[]
//
void SBTWrapper::add_indices(VkRayTracingPipelineCreateInfoKHR                     rayPipelineInfo,
							 const std::vector<VkRayTracingPipelineCreateInfoKHR>& libraries) {
	for (auto& i : m_index)
		i = {};

	uint32_t stage_idx = 0;


	for (size_t i = 0; i < libraries.size() + 1; i++) {
		// When using libraries, their groups and stages are appended after the groups and
		// stages defined in the main VkRayTracingPipelineCreateInfoKHR
		const auto& info = (i == 0) ? rayPipelineInfo : libraries[i - 1];
		// Libraries contain stages referencing their internal groups. When those groups
		// are used in the final pipeline we need to offset them to ensure each group has
		// a unique index
		uint32_t group_offset = stage_idx;
		// Finding the handle position of each group, splitting by raygen, miss and hit group
		for (uint32_t g = 0; g < info.groupCount; g++) {
			if (info.pGroups[g].type == VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR) {
				if (info.pStages[stage_idx].stage == VK_SHADER_STAGE_RAYGEN_BIT_KHR) {
					m_index[eRaygen].push_back(g + group_offset);
					stage_idx++;
				} else if (info.pStages[stage_idx].stage == VK_SHADER_STAGE_MISS_BIT_KHR) {
					m_index[eMiss].push_back(g + group_offset);
					stage_idx++;
				} else if (info.pStages[stage_idx].stage == VK_SHADER_STAGE_CALLABLE_BIT_KHR) {
					m_index[eCallable].push_back(g + group_offset);
					stage_idx++;
				}
			} else {
				m_index[eHit].push_back(g + group_offset);
				if (info.pGroups[g].closestHitShader != VK_SHADER_UNUSED_KHR)
					stage_idx++;
				if (info.pGroups[g].anyHitShader != VK_SHADER_UNUSED_KHR)
					stage_idx++;
				if (info.pGroups[g].intersectionShader != VK_SHADER_UNUSED_KHR)
					stage_idx++;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------
// This function creates 4 buffers, for raygen, miss, hit and callable shader.
// Each buffer will have the handle + 'data (if any)', .. n-times they have entries in the pipeline.
//
void SBTWrapper::create(VkPipeline  rt_pipeline,
						VkRayTracingPipelineCreateInfoKHR  ray_pipeline_info /*= {}*/,
						const std::vector<VkRayTracingPipelineCreateInfoKHR>& libraries_info /*= {}*/) {
	for (auto& b : m_buffer) {
		b.destroy();
	}

	// Get the total number of groups and handle index position
	uint32_t              total_group_cnt{ 0 };
	std::vector<uint32_t> group_cnt_per_input;
	// A pipeline is defined by at least its main VkRayTracingPipelineCreateInfoKHR, plus a number of external libraries
	group_cnt_per_input.reserve(1 + libraries_info.size());
	if (ray_pipeline_info.sType == VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR) {
		add_indices(ray_pipeline_info, libraries_info);
		group_cnt_per_input.push_back(ray_pipeline_info.groupCount);
		total_group_cnt += ray_pipeline_info.groupCount;
		for (const auto& lib : libraries_info) {
			group_cnt_per_input.push_back(lib.groupCount);
			total_group_cnt += lib.groupCount;
		}
	} else {
		// Find how many groups when added manually, by finding the largest index and adding 1
		// See also addIndex for manual entries
		for (auto& i : m_index) {
			if (!i.empty())
				total_group_cnt = std::max(total_group_cnt, *std::max_element(std::begin(i), std::end(i)));
		}
		total_group_cnt++;
		group_cnt_per_input.push_back(total_group_cnt);
	}

	// Fetch all the shader handles used in the pipeline, so that they can be written in the SBT
	uint32_t             sbtSize = total_group_cnt * m_handle_size;
	std::vector<uint8_t> shader_handle_storage(sbtSize);

	auto result =
		vkGetRayTracingShaderGroupHandlesKHR(m_ctx->device, rt_pipeline, 0, total_group_cnt, sbtSize, shader_handle_storage.data());
	// Find the max stride, minimum is the handle size + size of 'data (if any)' aligned to shaderGroupBaseAlignment
	auto findStride = [&](auto entry, auto& stride) {
		stride = align_up(m_handle_size, m_handle_alignment);  // minimum stride
		for (auto& e : entry) {
			// Find the largest data + handle size, all aligned
			uint32_t dataHandleSize =
				align_up(static_cast<uint32_t>(m_handle_size + e.second.size() * sizeof(uint8_t)), m_handle_alignment);
			stride = std::max(stride, dataHandleSize);
		}
	};
	findStride(m_data[eRaygen], m_stride[eRaygen]);
	findStride(m_data[eMiss], m_stride[eMiss]);
	findStride(m_data[eHit], m_stride[eHit]);
	findStride(m_data[eCallable], m_stride[eCallable]);

	// Buffer holding the staging information
	std::array<std::vector<uint8_t>, 4> stage;
	stage[eRaygen] = std::vector<uint8_t>(m_stride[eRaygen] * index_count(eRaygen));
	stage[eMiss] = std::vector<uint8_t>(m_stride[eMiss] * index_count(eMiss));
	stage[eHit] = std::vector<uint8_t>(m_stride[eHit] * index_count(eHit));
	stage[eCallable] = std::vector<uint8_t>(m_stride[eCallable] * index_count(eCallable));

	// Write the handles in the SBT buffer + data info (if any)
	auto copy_handles = [&](std::vector<uint8_t>& buffer, std::vector<uint32_t>& indices, uint32_t stride, auto& data) {
		auto* pbuffer = buffer.data();
		for (uint32_t index = 0; index < static_cast<uint32_t>(indices.size()); index++) {
			auto* pstart = pbuffer;
			// Copy the handle
			memcpy(pbuffer, shader_handle_storage.data() + (indices[index] * m_handle_size), m_handle_size);
			// If there is data for this group index, copy it too
			auto it = data.find(index);
			if (it != std::end(data)) {
				pbuffer += m_handle_size;
				memcpy(pbuffer, it->second.data(), it->second.size() * sizeof(uint8_t));
			}
			pbuffer = pstart + stride;  // Jumping to next group
		}
	};

	// Copy the handles/data to each staging buffer
	copy_handles(stage[eRaygen], m_index[eRaygen], m_stride[eRaygen], m_data[eRaygen]);
	copy_handles(stage[eMiss], m_index[eMiss], m_stride[eMiss], m_data[eMiss]);
	copy_handles(stage[eHit], m_index[eHit], m_stride[eHit], m_data[eHit]);
	copy_handles(stage[eCallable], m_index[eCallable], m_stride[eCallable], m_data[eCallable]);

	// Creating device local buffers where handles will be stored
	auto usage_flags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	auto mem_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	for (uint32_t i = 0; i < 4; i++) {
		if (!stage[i].empty()) {
			m_buffer[i].create(m_ctx, usage_flags, mem_flags, VK_SHARING_MODE_EXCLUSIVE, stage[i].size(), stage[i].data(), true);
		}
	}
}

VkDeviceAddress SBTWrapper::get_address(GroupType t) {
	if (!m_buffer[t].size) {
		return 0;
	}
	VkBufferDeviceAddressInfo i{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, m_buffer[t].handle };
	return vkGetBufferDeviceAddress(m_ctx->device, &i);  // Aligned on VkMemoryRequirements::alignment which includes shaderGroupBaseAlignment
}

const VkStridedDeviceAddressRegionKHR SBTWrapper::get_region(GroupType t) {
	return VkStridedDeviceAddressRegionKHR{ get_address(t), get_stride(t), get_size(t) };
}

const std::array<VkStridedDeviceAddressRegionKHR, 4> SBTWrapper::get_regions() {
	std::array<VkStridedDeviceAddressRegionKHR, 4> regions{ get_region(eRaygen), get_region(eMiss), get_region(eHit), get_region(eCallable) };
	return regions;
}
