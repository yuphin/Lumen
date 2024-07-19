#pragma once
#include "Integrator.h"
#include "shaders/integrators/sppm/sppm_commons.h"
class SPPM : public Integrator {
   public:
	SPPM(lumen::LumenInstance* scene, LumenScene* lumen_scene)
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

	lumen::Buffer sppm_data_buffer;
	lumen::Buffer atomic_data_buffer;
	lumen::Buffer photon_buffer;
	lumen::Buffer residual_buffer;
	lumen::Buffer counter_buffer;
	lumen::Buffer hash_buffer;
	lumen::Buffer tmp_col_buffer;

	SPPMConfig* config;
};
