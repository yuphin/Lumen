#pragma once
#include <unordered_map>
#include <assert.h> 

enum class LumenEvent {
	EVENT_SHADER_RELOAD

};

struct EventHandler {

	static std::unordered_map<LumenEvent, bool> event_table;


	static void set(LumenEvent event) {
		event_table[event] = true;
	}

	static void unset(LumenEvent event) {
		event_table[event] = false;
	}

	static bool get_event(LumenEvent event) {
		return event_table.count(event) > 0 && event_table[event];
	}



};
