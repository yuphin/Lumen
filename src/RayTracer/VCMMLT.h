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
	lumen::BufferOld bootstrap_buffer;
	lumen::BufferOld cdf_buffer;
	lumen::BufferOld cdf_sum_buffer;
	lumen::BufferOld seeds_buffer;
	lumen::BufferOld mlt_samplers_buffer;
	lumen::BufferOld light_primary_samples_buffer;
	lumen::BufferOld mlt_col_buffer;
	lumen::BufferOld chain_stats_buffer;
	lumen::BufferOld splat_buffer;
	lumen::BufferOld past_splat_buffer;
	lumen::BufferOld light_path_buffer;
	lumen::BufferOld bootstrap_cpu;
	lumen::BufferOld cdf_cpu;
	lumen::BufferOld tmp_col_buffer;
	lumen::BufferOld photon_buffer;
	lumen::BufferOld mlt_atomicsum_buffer;
	lumen::BufferOld mlt_residual_buffer;
	lumen::BufferOld counter_buffer;
	std::vector<lumen::BufferOld> block_sums;

	lumen::BufferOld light_path_cnt_buffer;
	int mutation_count;
	int light_path_rand_count;
	int sample_cnt = 0;

	VCMMLTConfig* config;
};
