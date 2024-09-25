#pragma once
#include "Integrator.h"
#include "shaders/integrators/smlt/smlt_commons.h"
class SMLT : public Integrator {
   public:
	SMLT(LumenScene* lumen_scene)
		: Integrator(lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), SMLTConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;

   private:
	void prefix_scan(int level, int num_elems, int& counter, lumen::RenderGraph* rg);
	PCMLT pc_ray{};
	PushConstantCompute pc_compute{};

	// SMLT buffers
	vk::Buffer* bootstrap_buffer;
	vk::Buffer* cdf_buffer;
	vk::Buffer* cdf_sum_buffer;
	vk::Buffer* seeds_buffer;
	vk::Buffer* mlt_samplers_buffer;
	vk::Buffer* light_primary_samples_buffer;
	vk::Buffer* cam_primary_samples_buffer;
	vk::Buffer* mlt_col_buffer;
	vk::Buffer* chain_stats_buffer;
	vk::Buffer* splat_buffer;
	vk::Buffer* past_splat_buffer;
	vk::Buffer* light_path_buffer;
	vk::Buffer* bootstrap_cpu;
	vk::Buffer* cdf_cpu;
	std::vector<vk::Buffer*> block_sums;

	vk::Buffer* connected_lights_buffer;
	vk::Buffer* tmp_seeds_buffer;
	vk::Buffer* tmp_lum_buffer;
	vk::Buffer* prob_carryover_buffer;
	vk::Buffer* light_path_cnt_buffer;
	vk::Buffer* light_splats_buffer;
	vk::Buffer* light_splat_cnts_buffer;

	float mutations_per_pixel;
	int num_mlt_threads;
	int num_bootstrap_samples;
	int mutation_count;
	int light_path_rand_count;
	int cam_path_rand_count;

	SMLTConfig* config;
};
