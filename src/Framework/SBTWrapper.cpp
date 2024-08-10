#include "../LumenPCH.h"
#include "SBTWrapper.h"
#include "VkUtils.h"
#include "CommandBuffer.h"
#include "PersistentResourceManager.h"

namespace lumen {

template <class T>
static constexpr T align_up(T x, size_t a) noexcept {
	return T((x + (T(a) - 1)) & ~T(a - 1));
}

void SBTWrapper::setup(uint32_t family_idx, const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rt_props) {
	m_queue_idx = family_idx;
	m_handle_size = rt_props.shaderGroupHandleSize;
	m_handle_alignment = rt_props.shaderGroupHandleAlignment;
}

void SBTWrapper::destroy() {
	for (vk::Buffer* b : m_buffer) {
		prm::remove(b);
	}
	for (auto& i : m_index) i = {};
}

void SBTWrapper::add_indices(VkRayTracingPipelineCreateInfoKHR pipeline_infos,
							 const std::vector<VkRayTracingPipelineCreateInfoKHR>& create_infos) {
	for (auto& i : m_index) i = {};

	uint32_t stage_idx = 0;

	for (size_t i = 0; i < create_infos.size() + 1; i++) {
		const auto& info = (i == 0) ? pipeline_infos : create_infos[i - 1];
		uint32_t group_offset = stage_idx;
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
				if (info.pGroups[g].closestHitShader != VK_SHADER_UNUSED_KHR) stage_idx++;
				if (info.pGroups[g].anyHitShader != VK_SHADER_UNUSED_KHR) stage_idx++;
				if (info.pGroups[g].intersectionShader != VK_SHADER_UNUSED_KHR) stage_idx++;
			}
		}
	}
}
void SBTWrapper::create(VkPipeline rt_pipeline, VkRayTracingPipelineCreateInfoKHR pipeline_info /*= {}*/,
						const std::vector<VkRayTracingPipelineCreateInfoKHR>& create_infos /*= {}*/) {
	for (vk::Buffer* b : m_buffer) {
		prm::remove(b);
	}

	uint32_t total_group_cnt{0};
	std::vector<uint32_t> group_cnt_per_input;
	group_cnt_per_input.reserve(1 + create_infos.size());
	if (pipeline_info.sType == VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR) {
		add_indices(pipeline_info, create_infos);
		group_cnt_per_input.push_back(pipeline_info.groupCount);
		total_group_cnt += pipeline_info.groupCount;
		for (const auto& lib : create_infos) {
			group_cnt_per_input.push_back(lib.groupCount);
			total_group_cnt += lib.groupCount;
		}
	} else {
		for (auto& i : m_index) {
			if (!i.empty()) total_group_cnt = std::max(total_group_cnt, *std::max_element(std::begin(i), std::end(i)));
		}
		total_group_cnt++;
		group_cnt_per_input.push_back(total_group_cnt);
	}
	uint32_t sbtSize = total_group_cnt * m_handle_size;
	std::vector<uint8_t> shader_handle_storage(sbtSize);

	auto result = vkGetRayTracingShaderGroupHandlesKHR(vk::context().device, rt_pipeline, 0, total_group_cnt, sbtSize,
													   shader_handle_storage.data());
	auto find_stride = [&](auto entry, auto& stride) {
		stride = align_up(m_handle_size, m_handle_alignment);  // minimum stride
		for (auto& e : entry) {
			uint32_t dataHandleSize =
				align_up(static_cast<uint32_t>(m_handle_size + e.second.size() * sizeof(uint8_t)), m_handle_alignment);
			stride = std::max(stride, dataHandleSize);
		}
	};
	find_stride(m_data[eRaygen], m_stride[eRaygen]);
	find_stride(m_data[eMiss], m_stride[eMiss]);
	find_stride(m_data[eHit], m_stride[eHit]);
	find_stride(m_data[eCallable], m_stride[eCallable]);

	std::array<std::vector<uint8_t>, 4> stage;
	stage[eRaygen] = std::vector<uint8_t>(m_stride[eRaygen] * index_count(eRaygen));
	stage[eMiss] = std::vector<uint8_t>(m_stride[eMiss] * index_count(eMiss));
	stage[eHit] = std::vector<uint8_t>(m_stride[eHit] * index_count(eHit));
	stage[eCallable] = std::vector<uint8_t>(m_stride[eCallable] * index_count(eCallable));

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
			pbuffer = pstart + stride;	// Jumping to next group
		}
	};

	copy_handles(stage[eRaygen], m_index[eRaygen], m_stride[eRaygen], m_data[eRaygen]);
	copy_handles(stage[eMiss], m_index[eMiss], m_stride[eMiss], m_data[eMiss]);
	copy_handles(stage[eHit], m_index[eHit], m_stride[eHit], m_data[eHit]);
	copy_handles(stage[eCallable], m_index[eCallable], m_stride[eCallable], m_data[eCallable]);

	VkBufferUsageFlags usage_flags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
	auto mem_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	for (uint32_t i = 0; i < 4; i++) {
		if (!stage[i].empty()) {
			m_buffer[i] = prm::get_buffer(
				{.usage = usage_flags,
				 .memory_type = vk::BufferType::GPU,
				 .size = stage[i].size(),
				 .data = stage[i].data()});
		}
	}
}

VkDeviceAddress SBTWrapper::get_address(GroupType t) {
	if (!m_buffer[t]->size) {
		return 0;
	}
	VkBufferDeviceAddressInfo i{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, m_buffer[t]->handle};
	return vkGetBufferDeviceAddress(vk::context().device, &i);
}

const VkStridedDeviceAddressRegionKHR SBTWrapper::get_region(GroupType t) {
	return VkStridedDeviceAddressRegionKHR{get_address(t), get_stride(t), get_size(t)};
}

const std::array<VkStridedDeviceAddressRegionKHR, 4> SBTWrapper::get_regions() {
	std::array<VkStridedDeviceAddressRegionKHR, 4> regions{get_region(eRaygen), get_region(eMiss), get_region(eHit),
														   get_region(eCallable)};
	return regions;
}

}  // namespace lumen
