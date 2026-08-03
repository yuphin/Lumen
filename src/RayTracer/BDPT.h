#pragma once
#include "shaders/integrators/bdpt/bdpt_commons.h"

namespace vk {
struct Buffer;
}  // namespace vk

struct Integrator;

struct BDPT {
	PCBDPT pc{};
	vk::Buffer* light_path_buffer = nullptr;
	vk::Buffer* camera_path_buffer = nullptr;
	vk::Buffer* path_counts_buffer = nullptr;
	vk::Buffer* color_storage_buffer = nullptr;
	bool enable_accumulation = true;
	bool isolate_strategy = false;
	i32 strategy_s = 0;
	i32 strategy_t = 2;
};

namespace bdpt {
void init(Integrator* integrator);
void render(Integrator* integrator);
bool update(Integrator* integrator);
bool gui(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
}  // namespace bdpt
