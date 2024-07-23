#include "../LumenPCH.h"
#include "EventPool.h"

namespace lumen::vk {
VkEvent EventPool::get_event(VkDevice device, VkCommandBuffer cmd) {
	// Note: Render graph may be executed with a command buffer that has VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	// Still, it looks like the VkCommandBuffer id is still the same across each frame.
	// TODO: Check if this is defined.
	VkEventCreateInfo event_create_info = {VK_STRUCTURE_TYPE_EVENT_CREATE_INFO};
	if (events_map.find(cmd) == events_map.end()) {
		events_map[cmd].events.push_back(VkEvent());
		events_map[cmd].available_event_idx = 1;
		vk::check(vkCreateEvent(device, &event_create_info, nullptr, &events_map[cmd].events.back()));
		return events_map[cmd].events.back();
	} else if (events_map[cmd].available_event_idx < events_map[cmd].events.size()) {
		// vk::check(vkCreateEvent(device, &event_create_info, nullptr,
		// &events_map[cmd].events[events_map[cmd].available_event_idx]));
		return events_map[cmd].events[events_map[cmd].available_event_idx++];
	} else {
		++events_map[cmd].available_event_idx;
		events_map[cmd].events.push_back(VkEvent());
		vk::check(vkCreateEvent(device, &event_create_info, nullptr, &events_map[cmd].events.back()));
		return events_map[cmd].events.back();
	}
}

void EventPool::reset_events(VkDevice device) {
	for (auto& [cmd, event] : events_map) {
		event.available_event_idx = 0;
	}
}

void EventPool::cleanup(VkDevice device) {
	for (const auto& [k, v] : events_map) {
		for (VkEvent event : v.events) {
			vkDestroyEvent(device, event, nullptr);
		}
	}
	events_map.clear();
}

}  // namespace lumen::vk
