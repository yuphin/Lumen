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
	lumen::BufferOld bootstrap_buffer;
	lumen::BufferOld cdf_buffer;
	lumen::BufferOld cdf_sum_buffer;
	lumen::BufferOld seeds_buffer;
	lumen::BufferOld mlt_samplers_buffer;
	lumen::BufferOld light_primary_samples_buffer;
	lumen::BufferOld cam_primary_samples_buffer;
	lumen::BufferOld connection_primary_samples_buffer;
	lumen::BufferOld mlt_col_buffer;
	lumen::BufferOld chain_stats_buffer;
	lumen::BufferOld splat_buffer;
	lumen::BufferOld past_splat_buffer;
	lumen::BufferOld light_path_buffer;
	lumen::BufferOld camera_path_buffer;

	lumen::BufferOld bootstrap_cpu;
	lumen::BufferOld cdf_cpu;

	std::vector<lumen::BufferOld> block_sums;

	int mutation_count;
	int light_path_rand_count;
	int cam_path_rand_count;
	int connect_path_rand_count;

	PSSMLTConfig* config;
};
