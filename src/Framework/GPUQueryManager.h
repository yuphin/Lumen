#pragma once
#include <vulkan/vulkan_core.h>
#include "Utils.h"
#include <string>
namespace GPUQueryManager {

struct TimestampData {
	std::string names[512];
	uint64_t timestamps[1024];
	uint32_t size;
};
void begin(VkCommandBuffer cmd, const char* name);
void end(VkCommandBuffer cmd);
void collect(uint32_t curr_frame_idx);
const TimestampData& get();
}  // namespace GPUQueryManager