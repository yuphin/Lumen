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
	Buffer direct_lighting_buffer;
	Buffer gris_gbuffer;
	Buffer gris_reservoir_buffer;
	PCReSTIRPT pc_ray{};
	bool enable_accumulation = false;

	ReSTIRGIConfig* config;
};
