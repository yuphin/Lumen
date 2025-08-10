#pragma once
#include "Base/Utils.h"
namespace GPUQueryManager {

struct TimestampData {
	std::string name;
	TimestampData* parent = nullptr;
	u32 start_timestamp_idx = 0;
	u32 end_timestamp_idx = 0;
};

void begin(VkCommandBuffer cmd, const char* name);
void end(VkCommandBuffer cmd);
void collect(u32 curr_frame_idx);
void collect();
void reset_data();

util::Slice<TimestampData> get();
uint64_t get_elapsed(const TimestampData& data);
uint64_t get_total_elapsed();
}  // namespace GPUQueryManager