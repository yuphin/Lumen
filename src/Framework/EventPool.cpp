#include "../LumenPCH.h"
#include "EventPool.h"

namespace vk {

namespace event_pool {
struct Events {
	std::vector<VkEvent> events;
	size_t available_event_idx = -1;
};
std::unordered_map<VkCommandBuffer, Events> _events_map;

VkEvent get_event(VkCommandBuffer cmd) {
	// Note: Render graph may be executed with a command buffer that has VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	// Still, it looks like the VkCommandBuffer id is still the same across each frame.
	// TODO: Check if this is defined.
	VkEventCreateInfo event_create_info = {VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};
	if (_events_map.find(cmd) == _events_map.end()) {
		_events_map[cmd].events.push_back(VkEvent());
		_events_map[cmd].available_event_idx = 1;
		vk::check(vkCreateEvent(context().device, &event_create_info, nullptr, &_events_map[cmd].events.back()));
		return _events_map[cmd].events.back();
	} else if (_events_map[cmd].available_event_idx < _events_map[cmd].events.size()) {
		// vk::check(vkCreateEvent(device, &event_create_info, nullptr,
		// &_events_map[cmd].events[_events_map[cmd].available_event_idx]));
		return _events_map[cmd].events[_events_map[cmd].available_event_idx++];
	} else {
		++_events_map[cmd].available_event_idx;
		_events_map[cmd].events.push_back(VkEvent());
		vk::check(vkCreateEvent(context().device, &event_create_info, nullptr, &_events_map[cmd].events.back()));
		return _events_map[cmd].events.back();
	}
}

void reset_events() {
	for (auto& [cmd, event] : _events_map) {
		event.available_event_idx = 0;
	}
}

void cleanup() {
	for (const auto& [k, v] : _events_map) {
		for (VkEvent event : v.events) {
			vkDestroyEvent(context().device, event, nullptr);
		}
	}
	_events_map.clear();
}

}  // namespace event_pool

}  // namespace vk
