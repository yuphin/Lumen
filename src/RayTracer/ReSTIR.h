#pragma once
#include "Integrator.h"
class ReSTIR : public Integrator {
   public:
	ReSTIR(LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), ReSTIRConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;

   private:
	Buffer g_buffer;
	Buffer passthrough_reservoir_buffer;
	Buffer temporal_reservoir_buffer;
	Buffer spatial_reservoir_buffer;
	Buffer tmp_col_buffer;
	PushConstantRay pc_ray{};
	VkDescriptorPool desc_pool;
	VkDescriptorSetLayout desc_set_layout;
	VkDescriptorSet desc_set;

	bool do_spatiotemporal = false;
	ReSTIRConfig* config;
};
