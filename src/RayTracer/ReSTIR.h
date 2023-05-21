#pragma once
#include "Integrator.h"
class ReSTIR : public Integrator {
   public:
	ReSTIR(LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), ReSTIRConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual bool gui() override;
	virtual void destroy() override;

   private:
	Buffer g_buffer;
	Buffer passthrough_reservoir_buffer;
	Buffer temporal_reservoir_buffer;
	Buffer spatial_reservoir_buffer;
	Buffer tmp_col_buffer;
	PCReSTIR pc_ray{};
	bool do_spatiotemporal = false;
	bool enable_accumulation = false;
	ReSTIRConfig* config;
};
