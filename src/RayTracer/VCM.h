#pragma once
#include "Integrator.h"
#include "shaders/integrators/vcm/vcm_commons.h"
class VCM : public Integrator {
   public:
	VCM(lumen::LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), VCMConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;

   private:
	PCVCM pc_ray{};
	VkDescriptorPool desc_pool{};
	VkDescriptorSetLayout desc_set_layout{};
	VkDescriptorSet desc_set;
	lumen::BufferOld photon_buffer;
	lumen::BufferOld vcm_light_vertices_buffer;
	lumen::BufferOld light_path_cnt_buffer;
	lumen::BufferOld color_storage_buffer;
	lumen::BufferOld vcm_reservoir_buffer;
	lumen::BufferOld light_samples_buffer;
	lumen::BufferOld should_resample_buffer;
	lumen::BufferOld light_state_buffer;
	lumen::BufferOld angle_struct_buffer;
	lumen::BufferOld angle_struct_cpu_buffer;
	lumen::BufferOld avg_buffer;
	bool do_spatiotemporal = false;
	uint32_t total_frame_cnt = 0;

	VCMConfig* config;
};
