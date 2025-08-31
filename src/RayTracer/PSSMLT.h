#pragma once
#include "Integrator.h"
#include "shaders/integrators/pssmlt/pssmlt_commons.h"
class PSSMLT final : public Integrator {
   public:
	PSSMLT(const vk::BVH& tlas) : Integrator(tlas) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy(bool resize) override;

   private:
	void prefix_scan(i32 level, i32 num_elems, i32& counter, lm::RenderGraph* rg);
	PCMLT pc_ray{};
	PushConstantCompute pc_compute{};
	// PSSMLT buffers
	vk::Buffer* bootstrap_buffer;
	vk::Buffer* cdf_buffer;
	vk::Buffer* cdf_sum_buffer;
	vk::Buffer* seeds_buffer;
	vk::Buffer* mlt_samplers_buffer;
	vk::Buffer* light_primary_samples_buffer;
	vk::Buffer* cam_primary_samples_buffer;
	vk::Buffer* connection_primary_samples_buffer;
	vk::Buffer* mlt_col_buffer;
	vk::Buffer* chain_stats_buffer;
	vk::Buffer* splat_buffer;
	vk::Buffer* past_splat_buffer;
	vk::Buffer* light_path_buffer;
	vk::Buffer* camera_path_buffer;

	vk::Buffer* bootstrap_cpu;
	vk::Buffer* cdf_cpu;

	std::vector<vk::Buffer*> block_sums;

	i32 mutation_count;
	i32 light_path_rand_count;
	i32 cam_path_rand_count;
	i32 connect_path_rand_count;
};
