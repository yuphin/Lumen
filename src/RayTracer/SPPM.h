#pragma once
#include "Integrator.h"
class SPPM : public Integrator {
public:
	SPPM(LumenInstance* scene) : Integrator(scene) {}
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
	PushConstantRay pc_ray{};
	VkDescriptorPool desc_pool;
	VkDescriptorSetLayout desc_set_layout;
	VkDescriptorSet desc_set;
	std::unique_ptr<Pipeline> sppm_light_pipeline;
	std::unique_ptr<Pipeline> sppm_eye_pipeline;
	// Compute pipelines
	std::unique_ptr<Pipeline> min_pipeline = nullptr;
	std::unique_ptr<Pipeline> min_reduce_pipeline = nullptr;
	std::unique_ptr<Pipeline> max_pipeline = nullptr;
	std::unique_ptr<Pipeline> max_reduce_pipeline = nullptr;
	std::unique_ptr<Pipeline> calc_bounds_pipeline = nullptr;

	Buffer sppm_data_buffer;
	Buffer atomic_data_buffer;
	Buffer photon_buffer;
	Buffer residual_buffer;
	Buffer counter_buffer;
	Buffer hash_buffer;
	Buffer atomic_data_cpu;
};

