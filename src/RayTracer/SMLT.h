#pragma once
#include "Integrator.h"
#include "shaders/integrators/smlt/smlt_commons.h"
class SMLT : public Integrator {
   public:
	SMLT(lumen::LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), SMLTConfig)) {}
	virtual void init() override;
	virtual void render() override;
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
	lumen::Buffer cam_primary_samples_buffer;
	lumen::Buffer mlt_col_buffer;
	lumen::Buffer chain_stats_buffer;
	lumen::Buffer splat_buffer;
	lumen::Buffer past_splat_buffer;
	lumen::Buffer light_path_buffer;
	lumen::Buffer bootstrap_cpu;
	lumen::Buffer cdf_cpu;
	std::vector<lumen::Buffer> block_sums;

	lumen::Buffer connected_lights_buffer;
	lumen::Buffer tmp_seeds_buffer;
	lumen::Buffer tmp_lum_buffer;
	lumen::Buffer prob_carryover_buffer;
	lumen::Buffer light_path_cnt_buffer;
	lumen::Buffer light_splats_buffer;
	lumen::Buffer light_splat_cnts_buffer;

	float mutations_per_pixel;
	int num_mlt_threads;
	int num_bootstrap_samples;
	int mutation_count;
	int light_path_rand_count;
	int cam_path_rand_count;

	SMLTConfig* config;
};
