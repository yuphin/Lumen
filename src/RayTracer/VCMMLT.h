#pragma once
#include "Integrator.h"
class VCMMLT : public Integrator {
   public:
	VCMMLT(LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), VCMMLTConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool gui() override;
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
	Buffer mlt_col_buffer;
	Buffer chain_stats_buffer;
	Buffer splat_buffer;
	Buffer past_splat_buffer;
	Buffer light_path_buffer;
	Buffer bootstrap_cpu;
	Buffer cdf_cpu;
	Buffer tmp_col_buffer;
	Buffer photon_buffer;
	Buffer mlt_atomicsum_buffer;
	Buffer mlt_residual_buffer;
	Buffer counter_buffer;
	std::vector<Buffer> block_sums;

	Buffer light_path_cnt_buffer;
	int mutation_count;
	int light_path_rand_count;
	int sample_cnt = 0;

	VCMMLTConfig* config;
};
