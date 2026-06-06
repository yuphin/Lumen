#pragma once
#include "shaders/integrators/smlt/smlt_commons.h"

struct Integrator;

struct SMLT {
	PCMLT pc_ray{};
	PushConstantCompute pc_compute{};
	vk::Buffer* bootstrap_buffer = nullptr;
	vk::Buffer* cdf_buffer = nullptr;
	vk::Buffer* cdf_sum_buffer = nullptr;
	vk::Buffer* seeds_buffer = nullptr;
	vk::Buffer* mlt_samplers_buffer = nullptr;
	vk::Buffer* light_primary_samples_buffer = nullptr;
	vk::Buffer* cam_primary_samples_buffer = nullptr;
	vk::Buffer* mlt_col_buffer = nullptr;
	vk::Buffer* chain_stats_buffer = nullptr;
	vk::Buffer* splat_buffer = nullptr;
	vk::Buffer* past_splat_buffer = nullptr;
	vk::Buffer* light_path_buffer = nullptr;
	vk::Buffer* bootstrap_cpu = nullptr;
	vk::Buffer* cdf_cpu = nullptr;
	lm::Array<vk::Buffer*> block_sums;
	vk::Buffer* connected_lights_buffer = nullptr;
	vk::Buffer* tmp_seeds_buffer = nullptr;
	vk::Buffer* tmp_lum_buffer = nullptr;
	vk::Buffer* prob_carryover_buffer = nullptr;
	vk::Buffer* light_path_cnt_buffer = nullptr;
	vk::Buffer* light_splats_buffer = nullptr;
	vk::Buffer* light_splat_cnts_buffer = nullptr;
	f32 mutations_per_pixel = 0;
	i32 num_mlt_threads = 0;
	i32 num_bootstrap_samples = 0;
	i32 mutation_count = 0;
	i32 light_path_rand_count = 0;
	i32 cam_path_rand_count = 0;
};

namespace smlt {
void prefix_scan(Integrator* integrator, i32 level, i32 num_elems, i32& counter, lm::RenderGraph* rg);
void init(Integrator* integrator);
void render(Integrator* integrator);
bool update(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
}  // namespace smlt
