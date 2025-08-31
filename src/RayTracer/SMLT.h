#pragma once
#include "Integrator.h"
#include "shaders/integrators/smlt/smlt_commons.h"
class SMLT final : public Integrator {
   public:
	SMLT(const vk::BVH& tlas) : Integrator(tlas) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy(bool resize) override;

   private:
	void prefix_scan(i32 level, i32 num_elems, i32& counter, lm::RenderGraph* rg);
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

	f32 mutations_per_pixel;
	i32 num_mlt_threads;
	i32 num_bootstrap_samples;
	i32 mutation_count;
	i32 light_path_rand_count;
	i32 cam_path_rand_count;
};
