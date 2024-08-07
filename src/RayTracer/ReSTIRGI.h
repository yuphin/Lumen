#pragma once
#include "Integrator.h"
#include "shaders/integrators/restir/gi/restirgi_commons.h"
class ReSTIRGI : public Integrator {
   public:
	ReSTIRGI(lumen::LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), ReSTIRGIConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual bool gui() override;
	virtual void destroy() override;

   private:
	lumen::Buffer restir_samples_buffer;
	lumen::Buffer restir_samples_old_buffer;
	lumen::Buffer temporal_reservoir_buffer;
	lumen::Buffer spatial_reservoir_buffer;
	lumen::Buffer tmp_col_buffer;
	PCReSTIRGI pc_ray{};
	bool do_spatiotemporal = false;
	bool enable_accumulation = false;

	ReSTIRGIConfig* config;
};
