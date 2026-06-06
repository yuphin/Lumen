#pragma once
#include "shaders/integrators/vcm/vcm_commons.h"

struct Integrator;

struct VCM {
	PCVCM pc_ray{};
	VkDescriptorPool desc_pool{};
	VkDescriptorSetLayout desc_set_layout{};
	vk::Buffer* photon_buffer = nullptr;
	vk::Buffer* vcm_light_vertices_buffer = nullptr;
	vk::Buffer* light_path_cnt_buffer = nullptr;
	vk::Buffer* color_storage_buffer = nullptr;
	vk::Buffer* vcm_reservoir_buffer = nullptr;
	vk::Buffer* light_samples_buffer = nullptr;
	vk::Buffer* should_resample_buffer = nullptr;
	vk::Buffer* light_state_buffer = nullptr;
	vk::Buffer* angle_struct_buffer = nullptr;
	vk::Buffer* avg_buffer = nullptr;
	bool do_spatiotemporal = false;
	bool use_vc = true;
};

namespace vcm {
void init(Integrator* integrator);
void render(Integrator* integrator);
bool update(Integrator* integrator);
bool gui(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
}  // namespace vcm
