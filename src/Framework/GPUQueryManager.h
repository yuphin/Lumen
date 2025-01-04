#pragma once
#include <vulkan/vulkan_core.h>
#include "Utils.h"
#include <string>
namespace GPUQueryManager {

struct TimestampData {
	std::string name;
	TimestampData* parent = nullptr;
	uint32_t start_timestamp_idx = 0;
	uint32_t end_timestamp_idx = 0;
};

void begin(VkCommandBuffer cmd, const char* name);
void end(VkCommandBuffer cmd);
void collect(uint32_t curr_frame_idx);
void collect();

util::Slice<TimestampData> get();
uint64_t get_elapsed(const TimestampData& data);
uint64_t get_total_elapsed();
}  // namespace GPUQueryManager