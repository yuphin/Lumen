#pragma once
#include "Integrator.h"
class SMLT : public Integrator {
public:
	SMLT(LumenInstance* scene) : Integrator(scene) {}
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
	std::unique_ptr<Pipeline> seed_light_pipeline;
	std::unique_ptr<Pipeline> seed_eye_pipeline;
	std::unique_ptr<Pipeline> preprocess_light_pipeline;
	std::unique_ptr<Pipeline> preprocess_eye_pipeline;
	std::unique_ptr<Pipeline> mutate_light_pipeline;
	std::unique_ptr<Pipeline> mutate_eye_pipeline;
	// Compute pipelines
	std::unique_ptr<Pipeline> calc_cdf_pipeline = nullptr;
	std::unique_ptr<Pipeline> select_seeds_pipeline = nullptr;
	std::unique_ptr<Pipeline> composite_pipeline = nullptr;
	std::unique_ptr<Pipeline> prefix_scan_pipeline = nullptr;
	std::unique_ptr<Pipeline> uniform_add_pipeline = nullptr;

	// SMLT buffers
	Buffer bootstrap_buffer;
	Buffer cdf_buffer;
	Buffer cdf_sum_buffer;
	Buffer seeds_buffer;
	Buffer mlt_samplers_buffer;
	Buffer light_primary_samples_buffer;
	Buffer cam_primary_samples_buffer;
	Buffer mlt_col_buffer;
	Buffer chain_stats_buffer;
	Buffer splat_buffer;
	Buffer past_splat_buffer;
	Buffer light_path_buffer;
	Buffer bootstrap_cpu;
	Buffer cdf_cpu;
	std::vector<Buffer> block_sums;

	Buffer connected_lights_buffer;
	Buffer tmp_seeds_buffer;
	Buffer tmp_lum_buffer;
	Buffer prob_carryover_buffer;
	Buffer light_path_cnt_buffer;


	int max_depth = 6;
	float mutations_per_pixel = 100.;
	vec3 sky_col = vec3(0, 0, 0);
	int num_mlt_threads = 200000;
	int num_bootstrap_samples = 100000;
	int mutation_count;
	int light_path_rand_count = 6 + 2 * max_depth;
	int cam_path_rand_count = 3 + 6 * max_depth;

};

