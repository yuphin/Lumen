/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */
#include "LumenPCH.h"
#include "Framework/Buffer.h"
#pragma once

class SBTWrapper {
public:
	enum GroupType {
		eRaygen,
		eMiss,
		eHit,
		eCallable
	};

	void setup(VulkanContext* ctx,
			   uint32_t family_idx,
			   const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rt_props);
	void destroy();
	void create(VkPipeline rtPipeline,
				VkRayTracingPipelineCreateInfoKHR pipeline_info = {},
				const std::vector<VkRayTracingPipelineCreateInfoKHR>& create_infos = {});

	void add_indices(VkRayTracingPipelineCreateInfoKHR pipeline_info,
					 const std::vector<VkRayTracingPipelineCreateInfoKHR>& create_infos = {});

	void add_index(GroupType t, uint32_t index) { m_index[t].push_back(index); }

	template <typename T>
	void add_data(GroupType t, uint32_t groupIndex, T& data) {
		add_data(t, groupIndex, (uint8_t*)&data, sizeof(T));
	}

	void add_data(GroupType t, uint32_t group_idx, uint8_t* data, size_t data_size) {
		std::vector<uint8_t> dst(data, data + data_size);
		m_data[t][group_idx] = dst;
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
