// Note: this wrapper is taken from NVPro Samples and adapted appropiately
#include "LumenPCH.h"
#include "Framework/Buffer.h"
#pragma once

class SBTWrapper {
public:
	enum GroupType
	{
		eRaygen,
		eMiss,
		eHit,
		eCallable
	};

	void setup(VulkanContext* ctx,
		uint32_t familyIndex,
		const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtProperties);
	void destroy();

	// To call after the ray tracer pipeline creation
	// The rayPipelineInfo parameter is the structure used to define the pipeline,
	// while librariesInfo describe the potential input pipeline libraries
	void create(VkPipeline rtPipeline,
		VkRayTracingPipelineCreateInfoKHR rayPipelineInfo = {},
		const std::vector<VkRayTracingPipelineCreateInfoKHR>& librariesInfo = {});

	// Optional, to be used in combination with addIndex. Leave create() `rayPipelineInfo`
	// and 'librariesInfo' empty.  The rayPipelineInfo parameter is the structure used to
	// define the pipeline, while librariesInfo describe the potential input pipeline libraries
	void add_indices(VkRayTracingPipelineCreateInfoKHR                     rayPipelineInfo,
		const std::vector<VkRayTracingPipelineCreateInfoKHR>& libraries = {});

	// Pushing back a GroupType and the handle pipeline index to use
	// i.e addIndex(eHit, 3) is pushing a Hit shader group using the 3rd entry in the pipeline
	void add_index(GroupType t, uint32_t index) { m_index[t].push_back(index); }

	// Adding 'Shader Record' data to the group index.
	// i.e. addData(eHit, 0, myValue) is adding 'myValue' to the HIT group 0.
	template <typename T>
	void add_data(GroupType t, uint32_t groupIndex, T& data)
	{
		add_data(t, groupIndex, (uint8_t*)&data, sizeof(T));
	}

	void add_data(GroupType t, uint32_t groupIndex, uint8_t* data, size_t dataSize)
	{
		std::vector<uint8_t> dst(data, data + dataSize);
		m_data[t][groupIndex] = dst;
	}

	// Getters
	uint32_t index_count(GroupType t) { return static_cast<uint32_t>(m_index[t].size()); }
	uint32_t get_stride(GroupType t) { return m_stride[t]; }
	uint32_t get_size(GroupType t) { return get_stride(t) * index_count(t); }
	VkDeviceAddress get_address(GroupType t);
	const VkStridedDeviceAddressRegionKHR get_region(GroupType t);
	const std::array<VkStridedDeviceAddressRegionKHR, 4> get_regions();


	VulkanContext* m_ctx = nullptr;
private:
	using entry = std::unordered_map<uint32_t, std::vector<uint8_t>>;
	std::array<std::vector<uint32_t>, 4> m_index;
	std::array<Buffer, 4> m_buffer;
	std::array<uint32_t, 4> m_stride{ 0, 0, 0, 0 };
	std::array<entry, 4>  m_data;
	uint32_t m_handle_size{ 0 };
	uint32_t m_handle_alignment{ 0 };
	uint32_t m_queue_idx{ 0 };
};
