#pragma once
#include "shaders/integrators/bdpt/bdpt_commons.h"

struct Integrator;

struct BDPT {
	PCBDPT pc_ray{};
	vk::Buffer* light_path_buffer = nullptr;
	vk::Buffer* camera_path_buffer = nullptr;
	vk::Buffer* color_storage_buffer = nullptr;
};

namespace bdpt {
void init(Integrator* integrator);
void render(Integrator* integrator);
bool update(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
}  // namespace bdpt
