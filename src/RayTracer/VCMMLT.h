#pragma once
#include "shaders/integrators/vcmmlt/vcmmlt_commons.h"

struct Integrator;

struct VCMMLT {
	PCMLT pc{};
	PushConstantCompute pc_compute{};
	vk::Buffer* bootstrap_buffer = nullptr;
	vk::Buffer* cdf_buffer = nullptr;
	vk::Buffer* cdf_sum_buffer = nullptr;
	vk::Buffer* seeds_buffer = nullptr;
	vk::Buffer* mlt_samplers_buffer = nullptr;
	vk::Buffer* light_primary_samples_buffer = nullptr;
	vk::Buffer* mlt_col_buffer = nullptr;
	vk::Buffer* chain_stats_buffer = nullptr;
	vk::Buffer* splat_buffer = nullptr;
	vk::Buffer* past_splat_buffer = nullptr;
	vk::Buffer* light_path_buffer = nullptr;
	vk::Buffer* tmp_col_buffer = nullptr;
	vk::Buffer* photon_buffer = nullptr;
	vk::Buffer* mlt_atomicsum_buffer = nullptr;
	vk::Buffer* mlt_residual_buffer = nullptr;
	vk::Buffer* counter_buffer = nullptr;
	lm::Array<vk::Buffer*> block_sums;
	vk::Buffer* light_path_cnt_buffer = nullptr;
	i32 mutation_count = 0;
	i32 light_path_rand_count = 0;
	i32 sample_cnt = 0;
};

namespace vcmmlt {
void prefix_scan(Integrator* integrator, i32 level, i32 num_elems, i32& counter, lm::RenderGraph* rg);
void init(Integrator* integrator);
void render(Integrator* integrator);
bool gui(Integrator* integrator);
bool update(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
}  // namespace vcmmlt
