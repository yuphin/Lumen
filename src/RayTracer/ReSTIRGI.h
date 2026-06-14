#pragma once
#include "shaders/integrators/restir/gi/restirgi_commons.h"

namespace vk {
struct Buffer;
}  // namespace vk

struct Integrator;

struct ReSTIRGI {
	vk::Buffer* restir_samples_buffer = nullptr;
	vk::Buffer* restir_samples_old_buffer = nullptr;
	vk::Buffer* temporal_reservoir_buffer = nullptr;
	vk::Buffer* spatial_reservoir_buffer = nullptr;
	vk::Buffer* tmp_col_buffer = nullptr;
	PCReSTIRGI pc{};
	bool do_spatiotemporal = false;
	bool enable_accumulation = false;
};

namespace restirgi {
void init(Integrator* integrator);
void render(Integrator* integrator);
bool update(Integrator* integrator);
bool gui(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
}  // namespace restirgi
