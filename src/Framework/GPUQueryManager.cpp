#include "../LumenPCH.h"
#include "GPUQueryManager.h"

namespace GPUQueryManager {

TimestampData _data;
uint32_t _curr_query_idx = 0;
uint32_t _curr_pool_idx = 0;

void begin(VkCommandBuffer cmd, const char* name) {
	LUMEN_ASSERT(_curr_query_idx < 4096, "Query pool exhausted");
	_data.names[_curr_query_idx >> 1] = std::string(name);
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::context().query_pool_timestamps[_curr_pool_idx],
						_curr_query_idx++);
}
void end(VkCommandBuffer cmd) {
	LUMEN_ASSERT(_curr_query_idx < 4096, "Query pool exhausted");
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk::context().query_pool_timestamps[_curr_pool_idx],
						_curr_query_idx++);
}

void collect(uint32_t curr_frame_idx) {
	if(_curr_query_idx == 0) {
		return;
	}
	// Note: curr_frame_idx is the index of the command buffer that has finished its execution
	vkGetQueryPoolResults(vk::context().device, vk::context().query_pool_timestamps[curr_frame_idx], 0, _curr_query_idx,
						  sizeof(uint64_t) * _curr_query_idx, _data.timestamps, sizeof(uint64_t),
						  VK_QUERY_RESULT_64_BIT);
	_curr_pool_idx = curr_frame_idx;
	vkResetQueryPool(vk::context().device, vk::context().query_pool_timestamps[curr_frame_idx], 0, 4096);

	_data.size = _curr_query_idx;
	_curr_query_idx = 0;
}

void collect() { collect(_curr_pool_idx); }

const TimestampData& get() { return _data; }
}  // namespace GPUQueryManager
