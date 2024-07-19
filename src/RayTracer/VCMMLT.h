#pragma once
#include "Integrator.h"
#include "shaders/integrators/vcmmlt/vcmmlt_commons.h"
class VCMMLT : public Integrator {
   public:
	VCMMLT(lumen::LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), VCMMLTConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool gui() override;
	virtual bool update() override;
	virtual void destroy() override;

   private:
	void prefix_scan(int level, int num_elems, int& counter, lumen::RenderGraph* rg);
	PCMLT pc_ray{};
	PushConstantCompute pc_compute{};
	// SMLT buffers
	lumen::Buffer bootstrap_buffer;
	lumen::Buffer cdf_buffer;
	lumen::Buffer cdf_sum_buffer;
	lumen::Buffer seeds_buffer;
	lumen::Buffer mlt_samplers_buffer;
	lumen::Buffer light_primary_samples_buffer;
	lumen::Buffer mlt_col_buffer;
	lumen::Buffer chain_stats_buffer;
	lumen::Buffer splat_buffer;
	lumen::Buffer past_splat_buffer;
	lumen::Buffer light_path_buffer;
	lumen::Buffer bootstrap_cpu;
	lumen::Buffer cdf_cpu;
	lumen::Buffer tmp_col_buffer;
	lumen::Buffer photon_buffer;
	lumen::Buffer mlt_atomicsum_buffer;
	lumen::Buffer mlt_residual_buffer;
	lumen::Buffer counter_buffer;
	std::vector<lumen::Buffer> block_sums;

	lumen::Buffer light_path_cnt_buffer;
	int mutation_count;
	int light_path_rand_count;
	int sample_cnt = 0;

	VCMMLTConfig* config;
};
