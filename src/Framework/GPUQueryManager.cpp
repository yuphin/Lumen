#include "../LumenPCH.h"
#include "GPUQueryManager.h"

namespace GPUQueryManager {

uint32_t _curr_pool_idx = 0;

uint64_t _queries[4096];
uint32_t _curr_query_idx = 0;
uint32_t _num_collected_queries = 0;

TimestampData _data[4096];
uint32_t _curr_timestamp_idx = 0;
uint32_t _num_collected_timestamps = 0;

std::vector<uint32_t> _timestamp_stack;

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

void collect(uint32_t curr_frame_idx) {
	_num_collected_timestamps = _curr_timestamp_idx;
	_num_collected_queries = _curr_query_idx;
	// Note: curr_frame_idx is the index of the command buffer that has finished its execution
	if (_curr_query_idx > 0) {
		vkGetQueryPoolResults(vk::context().device, vk::context().query_pool_timestamps[curr_frame_idx], 0,
							  _curr_query_idx, sizeof(uint64_t) * _curr_query_idx, _queries, sizeof(uint64_t),
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
uint64_t get_elapsed(const TimestampData& data) {
	return _queries[data.end_timestamp_idx] - _queries[data.start_timestamp_idx];
}

uint64_t get_total_elapsed() {
	return _num_collected_queries == 0 ? 0 : _queries[_num_collected_queries - 1] - _queries[0];
}
}  // namespace GPUQueryManager
