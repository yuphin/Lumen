#include "EventPool.h"
#include "Framework/Base/HashMap.h"
#include "Framework/VulkanContext.h"
namespace vk {

namespace event_pool {
static constexpr u64 MAX_EVENTS_PER_COMMAND_BUFFER = 65536;
static constexpr u64 INITIAL_COMMAND_BUFFER_CAPACITY = 64;

struct Events {
	lm::Array<VkEvent> events;
	u64 available_event_idx = 0;
};

static lm::Arena* _event_arena = nullptr;
static lm::HashMap<VkCommandBuffer, Events> _events_map;

static void ensure_initialized() {
	if (_event_arena) {
		return;
	}
	_event_arena = lm::arena_create(CSTR("Event Pool Arena"), MB(1), KB(64));
	_events_map = lm::hash_map_create<VkCommandBuffer, Events>(_event_arena, INITIAL_COMMAND_BUFFER_CAPACITY);
}

VkEvent get_event(VkCommandBuffer cmd) {
	// VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT applies to each recording, not the command buffer object.
	// Resetting and re-recording the same command buffer preserves its handle; freeing it does not.
	ensure_initialized();
	auto* entry = _events_map.find(cmd);
	if (!entry) {
		Events events;
		events.events =
			lm::array_create<VkEvent>(_event_arena, MAX_EVENTS_PER_COMMAND_BUFFER, MAX_EVENTS_PER_COMMAND_BUFFER);
		entry = _events_map.insert(cmd, events);
	}

	Events& event_cache = entry->value;
	if (event_cache.available_event_idx < event_cache.events.size) {
		return event_cache.events[event_cache.available_event_idx++];
	}

	if (event_cache.events.size >= MAX_EVENTS_PER_COMMAND_BUFFER) {
		LUMEN_ERROR("Event pool exceeded %llu cached events for a command buffer", MAX_EVENTS_PER_COMMAND_BUFFER);
	}

	VkEventCreateInfo event_create_info = {VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};
	VkEvent& event = event_cache.events.push();
	vk::check(vkCreateEvent(context().device, &event_create_info, nullptr, &event));
	++event_cache.available_event_idx;
	return event;
}

void reset_events() {
	if (!_event_arena) {
		return;
	}
	for (lm::HashMapEntry<VkCommandBuffer, Events>& entry : _events_map) {
		entry.value.available_event_idx = 0;
	}
}

void cleanup() {
	if (!_event_arena) {
		return;
	}
	for (lm::HashMapEntry<VkCommandBuffer, Events>& entry : _events_map) {
		for (VkEvent event : entry.value.events) {
			vkDestroyEvent(context().device, event, nullptr);
		}
	}
	_events_map = {};
	lm::arena_destroy(_event_arena);
	_event_arena = nullptr;
}

}  // namespace event_pool

}  // namespace vk
