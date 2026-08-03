#pragma once
#include "Base/String.h"
#include "Base/Utils.h"
namespace GPUQueryManager {

struct TimestampData {
	lm::String name;
	TimestampData* parent = nullptr;
	u32 start_timestamp_idx = 0;
	u32 end_timestamp_idx = 0;
};

void begin(VkCommandBuffer cmd, const lm::String& name);
void end(VkCommandBuffer cmd);
void begin_aggregate(VkCommandBuffer cmd, const lm::String& name);
void end_aggregate(VkCommandBuffer cmd);
void collect(u32 curr_frame_idx);
void collect();
void reset_data();

util::Slice<TimestampData> get();
u64 get_elapsed(const TimestampData& data);
}  // namespace GPUQueryManager
