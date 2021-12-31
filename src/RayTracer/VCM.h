#pragma once
#include "Integrator.h"
class VCM : public Integrator {
public:
	VCM(LumenInstance* scene) : Integrator(scene) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;
private:
	void create_offscreen_resources();
	void create_descriptors();
	void create_blas();
	void create_tlas();
	void create_rt_pipelines();
	PushConstantRay pc_ray{};
	VkDescriptorPool desc_pool;
	VkDescriptorSetLayout desc_set_layout;
	VkDescriptorSet desc_set;
	std::unique_ptr<Pipeline> vcm_light_pipeline;
	std::unique_ptr<Pipeline> vcm_eye_pipeline;

	Buffer photon_buffer;
	Buffer vcm_light_vertices_buffer;
	Buffer light_path_cnt_buffer;
	Buffer color_storage_buffer;
};

