#pragma once
#include "Integrator.h"
#include "shaders/integrators/vcmmlt/vcmmlt_commons.h"
class VCMMLT final : public Integrator {
   public:
	VCMMLT(const vk::BVH& tlas) : Integrator(tlas) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool gui() override;
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
	i32 mutation_count;
	i32 light_path_rand_count;
	i32 sample_cnt = 0;
};
