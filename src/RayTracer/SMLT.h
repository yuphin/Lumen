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
	lumen::BufferOld bootstrap_buffer;
	lumen::BufferOld cdf_buffer;
	lumen::BufferOld cdf_sum_buffer;
	lumen::BufferOld seeds_buffer;
	lumen::BufferOld mlt_samplers_buffer;
	lumen::BufferOld light_primary_samples_buffer;
	lumen::BufferOld cam_primary_samples_buffer;
	lumen::BufferOld mlt_col_buffer;
	lumen::BufferOld chain_stats_buffer;
	lumen::BufferOld splat_buffer;
	lumen::BufferOld past_splat_buffer;
	lumen::BufferOld light_path_buffer;
	lumen::BufferOld bootstrap_cpu;
	lumen::BufferOld cdf_cpu;
	std::vector<lumen::BufferOld> block_sums;

	lumen::BufferOld connected_lights_buffer;
	lumen::BufferOld tmp_seeds_buffer;
	lumen::BufferOld tmp_lum_buffer;
	lumen::BufferOld prob_carryover_buffer;
	lumen::BufferOld light_path_cnt_buffer;
	lumen::BufferOld light_splats_buffer;
	lumen::BufferOld light_splat_cnts_buffer;

	float mutations_per_pixel;
	int num_mlt_threads;
	int num_bootstrap_samples;
	int mutation_count;
	int light_path_rand_count;
	int cam_path_rand_count;

	SMLTConfig* config;
};
