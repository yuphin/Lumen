#pragma once
#include "Integrator.h"
class SPPM : public Integrator {
   public:
	SPPM(LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), SPPMConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;

   private:
	PCSPPM pc_ray{};
	VkDescriptorPool desc_pool{};
	VkDescriptorSetLayout desc_set_layout{};
	VkDescriptorSet desc_set;

	Buffer sppm_data_buffer;
	Buffer atomic_data_buffer;
	Buffer photon_buffer;
	Buffer residual_buffer;
	Buffer counter_buffer;
	Buffer hash_buffer;
	Buffer tmp_col_buffer;

	SPPMConfig* config;
};
