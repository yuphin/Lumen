#pragma once
#include "Integrator.h"
#include "shaders/integrators/sppm/sppm_commons.h"
class SPPM final : public Integrator {
   public:
	SPPM(const vk::BVH& tlas) : Integrator(tlas) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy(bool resize) override;

   private:
	PCSPPM pc_ray{};
	VkDescriptorPool desc_pool{};
	VkDescriptorSetLayout desc_set_layout{};

	vk::Buffer* sppm_data_buffer;
	vk::Buffer* atomic_data_buffer;
	vk::Buffer* photon_buffer;
	vk::Buffer* residual_buffer;
	vk::Buffer* counter_buffer;
};
