#pragma once
#include <vulkan/vulkan_core.h>
#include "Utils.h"
#include <string>
namespace GPUQueryManager {

struct TimestampData {
	std::string names[2048];
	uint64_t timestamps[4096];
	uint32_t size;
};
void begin(VkCommandBuffer cmd, const char* name);
void end(VkCommandBuffer cmd);
void collect(uint32_t curr_frame_idx);
void collect();
const TimestampData& get();
}  // namespace GPUQueryManager