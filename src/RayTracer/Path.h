#pragma once
#include "shaders/integrators/path/path_commons.h"

struct Integrator;

struct Path {
	PCPath pc{};
	u32 path_length = 0;
	bool direct_lighting = true;
	bool enable_accumulation = true;
};

namespace path {
void init(Integrator* integrator);
void render(Integrator* integrator);
bool update(Integrator* integrator);
bool gui(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
}  // namespace path
