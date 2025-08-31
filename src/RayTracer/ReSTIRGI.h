#pragma once
#include "Integrator.h"
#include "shaders/integrators/restir/gi/restirgi_commons.h"
class ReSTIRGI final : public Integrator {
   public:
	ReSTIRGI(const vk::BVH& tlas) : Integrator(tlas) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual bool gui() override;
	virtual void destroy(bool resize) override;

   private:
	vk::Buffer* restir_samples_buffer;
	vk::Buffer* restir_samples_old_buffer;
	vk::Buffer* temporal_reservoir_buffer;
	vk::Buffer* spatial_reservoir_buffer;
	vk::Buffer* tmp_col_buffer;
	PCReSTIRGI pc_ray{};
	bool do_spatiotemporal = false;
	bool enable_accumulation = false;
};
