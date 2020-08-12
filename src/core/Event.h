#pragma once
#include "lmhpch.h"

enum class LumenEvent {
	EVENT_SHADER_RELOAD
};

struct EventHandler {

	static std::unordered_map<LumenEvent, bool> event_table;
	static std::vector<VkPipeline> obsolete_pipelines;

	static void set(LumenEvent event);
	static void unset(LumenEvent event);
	static bool consume_event(LumenEvent event);
};
