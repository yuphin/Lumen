#pragma once
#include "Integrator.h"
class BDPT : public Integrator {
public:
	BDPT(LumenInstance* scene, LumenScene* lumen_scene) :
		Integrator(scene, lumen_scene) {}
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
	PushConstantRay pc_ray{};
	VkDescriptorPool desc_pool;
	VkDescriptorSetLayout desc_set_layout;
	VkDescriptorSet desc_set;
	std::unique_ptr<Pipeline> bdpt_pipeline;
	Buffer light_path_buffer;
	Buffer camera_path_buffer;
	Buffer color_storage_buffer;
};

