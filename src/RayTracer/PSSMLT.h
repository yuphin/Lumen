#pragma once
#include "shaders/integrators/pssmlt/pssmlt_commons.h"

namespace vk {
struct Buffer;
}  // namespace vk

struct Integrator;

struct PSSMLT {
	PCMLT pc{};
	PushConstantCompute pc_compute{};
	vk::Buffer* bootstrap_buffer = nullptr;
	vk::Buffer* cdf_buffer = nullptr;
	vk::Buffer* cdf_sum_buffer = nullptr;
	vk::Buffer* seeds_buffer = nullptr;
	vk::Buffer* mlt_samplers_buffer = nullptr;
	vk::Buffer* light_primary_samples_buffer = nullptr;
	vk::Buffer* cam_primary_samples_buffer = nullptr;
	vk::Buffer* connection_primary_samples_buffer = nullptr;
	vk::Buffer* mlt_col_buffer = nullptr;
	vk::Buffer* chain_stats_buffer = nullptr;
	vk::Buffer* splat_buffer = nullptr;
	vk::Buffer* past_splat_buffer = nullptr;
	vk::Buffer* light_path_buffer = nullptr;
	vk::Buffer* camera_path_buffer = nullptr;
	vk::Buffer* bootstrap_cpu = nullptr;
	vk::Buffer* cdf_cpu = nullptr;
	lm::Array<vk::Buffer*> block_sums;
	i32 mutation_count = 0;
	i32 light_path_rand_count = 0;
	i32 cam_path_rand_count = 0;
	i32 connect_path_rand_count = 0;
};

namespace pssmlt {
void init(Integrator* integrator);
void render(Integrator* integrator);
bool update(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
}  // namespace pssmlt
