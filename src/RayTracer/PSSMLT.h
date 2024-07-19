#pragma once
#include "Integrator.h"
#include "shaders/integrators/pssmlt/pssmlt_commons.h"
class PSSMLT : public Integrator {
   public:
	PSSMLT(lumen::LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), PSSMLTConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;

   private:
	void prefix_scan(int level, int num_elems, int& counter, lumen::RenderGraph* rg);
	PCMLT pc_ray{};
	PushConstantCompute pc_compute{};
	// PSSMLT buffers
	lumen::Buffer bootstrap_buffer;
	lumen::Buffer cdf_buffer;
	lumen::Buffer cdf_sum_buffer;
	lumen::Buffer seeds_buffer;
	lumen::Buffer mlt_samplers_buffer;
	lumen::Buffer light_primary_samples_buffer;
	lumen::Buffer cam_primary_samples_buffer;
	lumen::Buffer connection_primary_samples_buffer;
	lumen::Buffer mlt_col_buffer;
	lumen::Buffer chain_stats_buffer;
	lumen::Buffer splat_buffer;
	lumen::Buffer past_splat_buffer;
	lumen::Buffer light_path_buffer;
	lumen::Buffer camera_path_buffer;

	lumen::Buffer bootstrap_cpu;
	lumen::Buffer cdf_cpu;

	std::vector<lumen::Buffer> block_sums;

	int mutation_count;
	int light_path_rand_count;
	int cam_path_rand_count;
	int connect_path_rand_count;

	PSSMLTConfig* config;
};
