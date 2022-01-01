#pragma once
#include "LumenPCH.h"
#include "Event.h"

std::unordered_map<LumenEvent, bool> EventHandler::event_table = {};
std::vector<PipelineTrace> EventHandler::obsolete_pipelines = {};
std::vector<uint32_t> EventHandler::event_histogram = {};

void EventHandler::begin() {
	event_histogram.resize(event_count);
}

void EventHandler::set(LumenEvent event) { event_table[event] = true; }

void EventHandler::unset(LumenEvent event) { event_table[event] = false; }

bool EventHandler::consume_event(LumenEvent event) {
	if (event_table[event]) {
		event_table[event] = false;
		event_histogram[(uint32_t)event]++;
		return true;
	}
	return false;
}

bool EventHandler::signaled(LumenEvent event) {
	return event_histogram[(int)event] > 0;
}

