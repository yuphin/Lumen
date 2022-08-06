#include "LumenPCH.h"
#include "EventPool.h"

VkEvent EventPool::get_event(VkDevice device, VkCommandBuffer cmd) {
	VkEventCreateInfo event_create_info = { VK_STRUCTURE_TYPE_EVENT_CREATE_INFO };
	if (events_map.find(cmd) == events_map.end()) {
		events_map[cmd].events.push_back(VkEvent());
		events_map[cmd].available_event_idx = 1;
		vk::check(vkCreateEvent(device, &event_create_info, nullptr, &events_map[cmd].events.back()));
		return events_map[cmd].events.back();
	} else if (events_map[cmd].available_event_idx < events_map[cmd].events.size()) {
		//vk::check(vkCreateEvent(device, &event_create_info, nullptr, &events_map[cmd].events[events_map[cmd].available_event_idx]));
		return events_map[cmd].events[events_map[cmd].available_event_idx++];
	} else {
		++events_map[cmd].available_event_idx;
		events_map[cmd].events.push_back(VkEvent());
		vk::check(vkCreateEvent(device, &event_create_info, nullptr, &events_map[cmd].events.back()));
		return events_map[cmd].events.back();
	}

}

void EventPool::reset_events(VkDevice device, VkCommandBuffer cmd) {
	if (events_map.find(cmd) == events_map.end()) {
		return;
	}
	auto& event = events_map[cmd];
	event.available_event_idx = 0;
}
