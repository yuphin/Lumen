#pragma once
#include "Integrator.h"
#include "shaders/integrators/vcm/vcm_commons.h"
class VCM final : public Integrator {
   public:
	VCM(LumenScene* lumen_scene, const vk::BVH& tlas)
		: Integrator(lumen_scene, tlas), config(CAST_CONFIG(lumen_scene->config.get(), VCMConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy(bool resize) override;

   private:
	PCVCM pc_ray{};
	VkDescriptorPool desc_pool{};
	VkDescriptorSetLayout desc_set_layout{};
	vk::Buffer* photon_buffer;
	vk::Buffer* vcm_light_vertices_buffer;
	vk::Buffer* light_path_cnt_buffer;
	vk::Buffer* color_storage_buffer;
	vk::Buffer* vcm_reservoir_buffer;
	vk::Buffer* light_samples_buffer;
	vk::Buffer* should_resample_buffer;
	vk::Buffer* light_state_buffer;
	vk::Buffer* angle_struct_buffer;
	vk::Buffer* avg_buffer;
	bool do_spatiotemporal = false;

	VCMConfig* config;
};
