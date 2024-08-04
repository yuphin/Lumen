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

	lumen::BufferOld sppm_data_buffer;
	lumen::BufferOld atomic_data_buffer;
	lumen::BufferOld photon_buffer;
	lumen::BufferOld residual_buffer;
	lumen::BufferOld counter_buffer;
	lumen::BufferOld hash_buffer;
	lumen::BufferOld tmp_col_buffer;

	SPPMConfig* config;
};
