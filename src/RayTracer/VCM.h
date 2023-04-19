#pragma once
#include "Integrator.h"
class VCM : public Integrator {
   public:
	VCM(LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), VCMConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;

   private:
	PCVCM pc_ray{};
	VkDescriptorPool desc_pool;
	VkDescriptorSetLayout desc_set_layout;
	VkDescriptorSet desc_set;
	Buffer photon_buffer;
	Buffer vcm_light_vertices_buffer;
	Buffer light_path_cnt_buffer;
	Buffer color_storage_buffer;
	Buffer vcm_reservoir_buffer;
	Buffer light_samples_buffer;
	Buffer should_resample_buffer;
	Buffer light_state_buffer;
	Buffer angle_struct_buffer;
	Buffer angle_struct_cpu_buffer;
	Buffer avg_buffer;
	bool do_spatiotemporal = false;
	uint32_t total_frame_cnt = 0;

	VCMConfig* config;
};
