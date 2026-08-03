#include "GPUQueryManager.h"
#include "Framework/VulkanContext.h"

namespace GPUQueryManager {

static constexpr u32 MAX_QUERY_COUNT = 4096;
static constexpr u32 MAX_TIMESTAMP_COUNT = 4096;
static constexpr u64 MAX_TIMESTAMP_NAME_BYTES = MB(1);

static u32 _curr_pool_idx = 0;

static u64 _queries[MAX_QUERY_COUNT];
static u32 _curr_query_idx = 0;
static u32 _num_collected_queries = 0;

static TimestampData _data[MAX_TIMESTAMP_COUNT];
static u32 _curr_timestamp_idx = 0;
static u32 _num_collected_timestamps = 0;

static u32 _timestamp_stack[MAX_TIMESTAMP_COUNT];
static u32 _timestamp_stack_size = 0;
static bool _aggregate_active = false;
static u32 _suppressed_scope_depth = 0;

static char _timestamp_name_data[MAX_TIMESTAMP_NAME_BYTES];
static u64 _timestamp_name_data_size = 0;

void begin(VkCommandBuffer cmd, const lm::String& name) {
	if (_aggregate_active) {
		_suppressed_scope_depth++;
		return;
	}
	LUMEN_ASSERT(_curr_query_idx + 2 <= MAX_QUERY_COUNT, "Query pool exhausted");
	LUMEN_ASSERT(_curr_timestamp_idx < MAX_TIMESTAMP_COUNT, "Timestamp data exhausted");
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, vk::context().query_pool_timestamps[_curr_pool_idx],
						_curr_query_idx);

	u64 name_size = name.size;
	LUMEN_ASSERT(_timestamp_name_data_size + name_size <= MAX_TIMESTAMP_NAME_BYTES, "Timestamp name data exhausted");
	char* name_data = _timestamp_name_data + _timestamp_name_data_size;
	memcpy(name_data, name.data, name_size);
	_timestamp_name_data_size += name_size;

	_data[_curr_timestamp_idx].name = lm::String(name_data, name_size);
	_data[_curr_timestamp_idx].start_timestamp_idx = _curr_query_idx;

	if (_timestamp_stack_size > 0) {
		_data[_curr_timestamp_idx].parent = &_data[_timestamp_stack[_timestamp_stack_size - 1]];
	}

	LUMEN_ASSERT(_timestamp_stack_size < ARRAY_LEN(_timestamp_stack), "Timestamp stack exhausted");
	_timestamp_stack[_timestamp_stack_size++] = _curr_timestamp_idx;
	_curr_timestamp_idx++;
	_curr_query_idx++;
}
void end(VkCommandBuffer cmd) {
	if (_aggregate_active) {
		LUMEN_ASSERT(_suppressed_scope_depth > 0, "Mismatched timestamp end inside aggregate scope");
		_suppressed_scope_depth--;
		return;
	}
	LUMEN_ASSERT(_curr_query_idx < MAX_QUERY_COUNT, "Query pool exhausted");
	vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, vk::context().query_pool_timestamps[_curr_pool_idx],
						_curr_query_idx);

	LUMEN_ASSERT(_timestamp_stack_size > 0, "Mismatched begin/end timestamps");
	TimestampData& data = _data[_timestamp_stack[_timestamp_stack_size - 1]];
	data.end_timestamp_idx = _curr_query_idx;
	_timestamp_stack_size--;
	_curr_query_idx++;
}

void begin_aggregate(VkCommandBuffer cmd, const lm::String& name) {
	LUMEN_ASSERT(!_aggregate_active, "GPU timestamp aggregate scopes cannot be nested");
	LUMEN_ASSERT(_suppressed_scope_depth == 0, "Suppressed timestamp scopes were not balanced");
	begin(cmd, name);
	_aggregate_active = true;
}

void end_aggregate(VkCommandBuffer cmd) {
	LUMEN_ASSERT(_aggregate_active, "Mismatched aggregate timestamp end");
	LUMEN_ASSERT(_suppressed_scope_depth == 0, "Cannot end aggregate with open timestamp scopes");
	_aggregate_active = false;
	end(cmd);
}

void collect(u32 curr_frame_idx) {
	LUMEN_ASSERT(!_aggregate_active, "Cannot collect timestamps inside an aggregate scope");
	LUMEN_ASSERT(_suppressed_scope_depth == 0, "Cannot collect timestamps with suppressed scopes open");
	LUMEN_ASSERT(_timestamp_stack_size == 0, "Cannot collect timestamps with open scopes");
	LUMEN_ASSERT(_curr_timestamp_idx * 2 == _curr_query_idx,
				 "Mismatched begin/end timestamps: %u timestamps, %u queries", _curr_timestamp_idx, _curr_query_idx);
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
	_timestamp_name_data_size = 0;
	_curr_pool_idx = curr_frame_idx;
	vkResetQueryPool(vk::context().device, vk::context().query_pool_timestamps[curr_frame_idx], 0, MAX_QUERY_COUNT);
}

void collect() { collect(_curr_pool_idx); }
void reset_data() { memset(_data, 0, sizeof(_data)); }
util::Slice<TimestampData> get() { return util::Slice<TimestampData>(_data, _num_collected_timestamps); }

// Assumes that collect has been called
u64 get_elapsed(const TimestampData& data) {
	return _queries[data.end_timestamp_idx] - _queries[data.start_timestamp_idx];
}

u64 get_total_elapsed() { return _num_collected_queries == 0 ? 0 : _queries[_num_collected_queries - 1] - _queries[0]; }
}  // namespace GPUQueryManager
