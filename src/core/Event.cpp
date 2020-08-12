#pragma once
#include "lmhpch.h"
#include "Event.h"

std::unordered_map<LumenEvent, bool> EventHandler::event_table = {};
std::vector<VkPipeline> EventHandler::obsolete_pipelines = {};

void EventHandler::set(LumenEvent event) {
	event_table[event] = true;
}

void EventHandler::unset(LumenEvent event) {
	event_table[event] = false;
}

bool EventHandler::consume_event(LumenEvent event) {
	if (event_table[event]) {
		event_table[event] = false;
		return true;
	}
	return false;
}
