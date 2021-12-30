#pragma once
#include "LumenPCH.h"
#include "Framework/Pipeline.h"

enum class LumenEvent { EVENT_SHADER_RELOAD };

struct EventHandler {
	static std::unordered_map<LumenEvent, bool> event_table;
	static std::vector<uint32_t> event_histogram;
	static std::vector<VkPipeline> obsolete_pipelines;
	static const int event_count = 1;
	static void begin();
	static void set(LumenEvent event);
	static void unset(LumenEvent event);
	static bool consume_event(LumenEvent event);
};
