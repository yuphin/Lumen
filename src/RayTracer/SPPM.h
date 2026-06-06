#pragma once
#include "shaders/integrators/sppm/sppm_commons.h"

struct Integrator;

struct SPPM {
	PCSPPM pc_ray{};
	VkDescriptorPool desc_pool{};
	VkDescriptorSetLayout desc_set_layout{};
	vk::Buffer* sppm_data_buffer = nullptr;
	vk::Buffer* atomic_data_buffer = nullptr;
	vk::Buffer* photon_buffer = nullptr;
	vk::Buffer* residual_buffer = nullptr;
	vk::Buffer* counter_buffer = nullptr;
};

namespace sppm {
void init(Integrator* integrator);
void render(Integrator* integrator);
bool update(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
}  // namespace sppm
