#pragma once
#include "Integrator.h"
#include "shaders/integrators/pssmlt/pssmlt_commons.h"
class PSSMLT : public Integrator {
   public:
	PSSMLT(LumenScene* lumen_scene, const vk::BVH& tlas)
		: Integrator(lumen_scene, tlas), config(CAST_CONFIG(lumen_scene->config.get(), PSSMLTConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;

   private:
	void prefix_scan(int level, int num_elems, int& counter, lumen::RenderGraph* rg);
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

	int mutation_count;
	int light_path_rand_count;
	int cam_path_rand_count;
	int connect_path_rand_count;

	PSSMLTConfig* config;
};
