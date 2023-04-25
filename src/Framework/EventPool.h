#pragma once
#include "../LumenPCH.h"
class EventPool {
   public:
	struct Events {
		std::vector<VkEvent> events;
		size_t available_event_idx = -1;
	};
	EventPool() = default;
	VkEvent get_event(VkDevice device, VkCommandBuffer cmd);
	void reset_events(VkDevice device);
	void cleanup(VkDevice device);

   private:
	std::unordered_map<VkCommandBuffer, Events> events_map;
};
