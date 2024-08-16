#pragma once
#include "Integrator.h"
#include "shaders/integrators/vcmmlt/vcmmlt_commons.h"
class VCMMLT : public Integrator {
   public:
	VCMMLT(LumenScene* lumen_scene, const vk::BVH& tlas)
		: Integrator(lumen_scene, tlas), config(CAST_CONFIG(lumen_scene->config.get(), VCMMLTConfig)) {}
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
	vk::Buffer* bootstrap_buffer;
	vk::Buffer* cdf_buffer;
	vk::Buffer* cdf_sum_buffer;
	vk::Buffer* seeds_buffer;
	vk::Buffer* mlt_samplers_buffer;
	vk::Buffer* light_primary_samples_buffer;
	vk::Buffer* mlt_col_buffer;
	vk::Buffer* chain_stats_buffer;
	vk::Buffer* splat_buffer;
	vk::Buffer* past_splat_buffer;
	vk::Buffer* light_path_buffer;
	vk::Buffer* tmp_col_buffer;
	vk::Buffer* photon_buffer;
	vk::Buffer* mlt_atomicsum_buffer;
	vk::Buffer* mlt_residual_buffer;
	vk::Buffer* counter_buffer;
	std::vector<vk::Buffer*> block_sums;

	vk::Buffer* light_path_cnt_buffer;
	int mutation_count;
	int light_path_rand_count;
	int sample_cnt = 0;

	VCMMLTConfig* config;
};
