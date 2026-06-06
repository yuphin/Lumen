#pragma once
#include "shaders/integrators/restir/di/restirdi_commons.h"

struct Integrator;

struct ReSTIR {
	vk::Buffer* g_buffer = nullptr;
	vk::Buffer* passthrough_reservoir_buffer = nullptr;
	vk::Buffer* temporal_reservoir_buffer = nullptr;
	vk::Buffer* spatial_reservoir_buffer = nullptr;
	vk::Buffer* tmp_col_buffer = nullptr;
	PCReSTIR pc_ray{};
	bool do_spatiotemporal = false;
	bool enable_accumulation = false;
};

namespace restir {
void init(Integrator* integrator);
void render(Integrator* integrator);
bool update(Integrator* integrator);
bool gui(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
}  // namespace restir
