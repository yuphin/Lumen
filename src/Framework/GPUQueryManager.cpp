#include "GPUQueryManager.h"
#include "Framework/VulkanContext.h"

namespace GPUQueryManager {

u32 _curr_pool_idx = 0;

u64 _queries[4096];
u32 _curr_query_idx = 0;
u32 _num_collected_queries = 0;

TimestampData _data[4096];
u32 _curr_timestamp_idx = 0;
u32 _num_collected_timestamps = 0;

std::vector<u32> _timestamp_stack;

void begin(VkCommandBuffer cmd, const char* name) {
	LUMEN_ASSERT(_curr_query_idx < 4096, "Query pool exhausted");
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::context().query_pool_timestamps[_curr_pool_idx],
						_curr_query_idx);

	_data[_curr_timestamp_idx].name = std::string(name);
	_data[_curr_timestamp_idx].start_timestamp_idx = _curr_query_idx;

	if (!_timestamp_stack.empty()) {
		_data[_curr_timestamp_idx].parent = &_data[_timestamp_stack.back()];
	}

	_timestamp_stack.push_back(_curr_timestamp_idx);
	_curr_timestamp_idx++;
	_curr_query_idx++;
}
void end(VkCommandBuffer cmd) {
	LUMEN_ASSERT(_curr_query_idx < 4096, "Query pool exhausted");
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk::context().query_pool_timestamps[_curr_pool_idx],
						_curr_query_idx);

	LUMEN_ASSERT(!_timestamp_stack.empty(), "Mismatched begin/end timestamps");
	TimestampData& data = _data[_timestamp_stack.back()];
	data.end_timestamp_idx = _curr_query_idx;
	_timestamp_stack.pop_back();
	_curr_query_idx++;
}

void collect(u32 curr_frame_idx) {
	_num_collected_timestamps = _curr_timestamp_idx;
	_num_collected_queries = _curr_query_idx;
	// Note: curr_frame_idx is the index of the command buffer that has finished its execution
	if (_curr_query_idx > 0) {
		vkGetQueryPoolResults(vk::context().device, vk::context().query_pool_timestamps[curr_frame_idx], 0,
							  _curr_query_idx, sizeof(u64) * _curr_query_idx, _queries, sizeof(u64),
							  VK_QUERY_RESULT_64_BIT);
		_curr_query_idx = 0;
		_curr_timestamp_idx = 0;
	}
	_curr_pool_idx = curr_frame_idx;
	vkResetQueryPool(vk::context().device, vk::context().query_pool_timestamps[curr_frame_idx], 0, 4096);
}

void collect() { collect(_curr_pool_idx); }
void reset_data() { memset(_data, 0, sizeof(TimestampData) * 4096); }
util::Slice<TimestampData> get() { return util::Slice<TimestampData>(_data, _num_collected_timestamps); }

// Assumes that collect has been called
u64 get_elapsed(const TimestampData& data) {
	return _queries[data.end_timestamp_idx] - _queries[data.start_timestamp_idx];
}

u64 get_total_elapsed() {
	return _num_collected_queries == 0 ? 0 : _queries[_num_collected_queries - 1] - _queries[0];
}
}  // namespace GPUQueryManager
