#pragma once
#include "shaders/integrators/irradiance_cache/ir_commons.h"

namespace vk {
struct Buffer;
}  // namespace vk

struct Integrator;

struct IrradianceCache {
	vk::Buffer* gbuffer = nullptr;
	vk::Buffer* transformations_buffer = nullptr;
	vk::Buffer* surfel_spawn_list_buffer = nullptr;
	vk::Buffer* surfel_spawn_count_buffer = nullptr;
	vk::Buffer* surfel_pool_buffer = nullptr;
	vk::Buffer* surfel_free_stack_counter_buffer = nullptr;
	vk::Buffer* surfel_free_stack_buffer = nullptr;
	vk::Buffer* grid_cell_counts_buffer = nullptr;
	vk::Buffer* grid_cell_indices_buffer = nullptr;
	vk::Buffer* grid_prefix_sum_scratch_buffer = nullptr;

	vk::Buffer* surfel_samples_buffer = nullptr;
	PCIRCache pc{};
	bool direct_lighting = false;
	bool debug_mode = false;
	bool pause_surfel_spawn = false;
	bool use_camera_relative_surfel_size = true;
	u32 total_frame_idx = 0;
	u32 rays_per_surfel = 8;
};

namespace ircache {
void init(Integrator* integrator);
void render(Integrator* integrator);
bool update(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
bool gui(Integrator* integrator);
}  // namespace ircache
