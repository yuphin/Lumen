#pragma once
#include "Integrator.h"
class PSSMLT : public Integrator {
   public:
	PSSMLT(LumenInstance* scene, LumenScene* lumen_scene) : Integrator(scene, lumen_scene) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;
	virtual void reload() override;

   private:
	void create_offscreen_resources();
	void create_descriptors();
	void create_blas();
	void create_tlas();
	void create_rt_pipelines();
	void create_compute_pipelines();
	void prefix_scan(int level, int num_elems, CommandBuffer& cmd);
	PushConstantRay pc_ray{};
	PushConstantCompute pc_compute{};
	VkDescriptorPool desc_pool;
	VkDescriptorSetLayout desc_set_layout;
	VkDescriptorSet desc_set;
	std::unique_ptr<Pipeline> seed_pipeline;
	std::unique_ptr<Pipeline> preprocess_pipeline;
	std::unique_ptr<Pipeline> mutate_pipeline;
	// Compute pipelines
	std::unique_ptr<Pipeline> calc_cdf_pipeline = nullptr;
	std::unique_ptr<Pipeline> select_seeds_pipeline = nullptr;
	std::unique_ptr<Pipeline> composite_pipeline = nullptr;
	std::unique_ptr<Pipeline> prefix_scan_pipeline = nullptr;
	std::unique_ptr<Pipeline> uniform_add_pipeline = nullptr;

	// PSSMLT buffers
	Buffer bootstrap_buffer;
	Buffer cdf_buffer;
	Buffer cdf_sum_buffer;
	Buffer seeds_buffer;
	Buffer mlt_samplers_buffer;
	Buffer light_primary_samples_buffer;
	Buffer cam_primary_samples_buffer;
	Buffer connection_primary_samples_buffer;
	Buffer mlt_col_buffer;
	Buffer chain_stats_buffer;
	Buffer splat_buffer;
	Buffer past_splat_buffer;
	Buffer light_path_buffer;
	Buffer camera_path_buffer;

	Buffer bootstrap_cpu;
	Buffer cdf_cpu;

	std::vector<Buffer> block_sums;

	int mutation_count;
	int light_path_rand_count;
	int cam_path_rand_count;
	int connect_path_rand_count;
};
