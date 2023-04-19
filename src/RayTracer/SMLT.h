#pragma once
#include "Integrator.h"
class SMLT : public Integrator {
   public:
	SMLT(LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), SMLTConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;

   private:
	void prefix_scan(int level, int num_elems, int& counter, RenderGraph* rg);
	PCMLT pc_ray{};
	PushConstantCompute pc_compute{};

	// SMLT buffers
	Buffer bootstrap_buffer;
	Buffer cdf_buffer;
	Buffer cdf_sum_buffer;
	Buffer seeds_buffer;
	Buffer mlt_samplers_buffer;
	Buffer light_primary_samples_buffer;
	Buffer cam_primary_samples_buffer;
	Buffer mlt_col_buffer;
	Buffer chain_stats_buffer;
	Buffer splat_buffer;
	Buffer past_splat_buffer;
	Buffer light_path_buffer;
	Buffer bootstrap_cpu;
	Buffer cdf_cpu;
	std::vector<Buffer> block_sums;

	Buffer connected_lights_buffer;
	Buffer tmp_seeds_buffer;
	Buffer tmp_lum_buffer;
	Buffer prob_carryover_buffer;
	Buffer light_path_cnt_buffer;
	Buffer light_splats_buffer;
	Buffer light_splat_cnts_buffer;

	float mutations_per_pixel;
	int num_mlt_threads;
	int num_bootstrap_samples;
	int mutation_count;
	int light_path_rand_count;
	int cam_path_rand_count;

	SMLTConfig* config;
};
