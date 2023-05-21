#pragma once
#include "Integrator.h"
class ReSTIRPT : public Integrator {
   public:
	ReSTIRPT(LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), ReSTIRGIConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;
	virtual bool gui() override;

   private:
	Buffer restir_samples_buffer;
	Buffer restir_samples_old_buffer;
	Buffer temporal_reservoir_buffer;
	Buffer spatial_reservoir_buffer;
	Buffer tmp_col_buffer;
	PCReSTIRPT pc_ray{};
	bool do_spatiotemporal = false;
	bool enable_accumulation = false;

	ReSTIRGIConfig* config;
};
