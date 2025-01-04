#pragma once
#include "Integrator.h"
#include "shaders/integrators/restir/di/restirdi_commons.h"
class ReSTIR final: public Integrator {
   public:
	ReSTIR(LumenScene* lumen_scene, const vk::BVH& tlas)
		: Integrator(lumen_scene, tlas), config(CAST_CONFIG(lumen_scene->config.get(), ReSTIRConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual bool gui() override;
	virtual void destroy(bool resize) override;

   private:
	vk::Buffer* g_buffer;
	vk::Buffer* passthrough_reservoir_buffer;
	vk::Buffer* temporal_reservoir_buffer;
	vk::Buffer* spatial_reservoir_buffer;
	vk::Buffer* tmp_col_buffer;
	PCReSTIR pc_ray{};
	bool do_spatiotemporal = false;
	bool enable_accumulation = false;
	ReSTIRConfig* config;
};
