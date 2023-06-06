#pragma once
#include "Integrator.h"
#include "shaders/integrators/restir/gris/gris_commons.h"
using namespace RestirPT;
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
	bool enable_accumulation = true;
	bool direct_lighting = false;
	bool enable_rr = false;
	bool enable_spatial_reuse = true; 
	bool show_reconnection_radiance = false;
	float spatial_reuse_radius = 32.0f;
	float min_vertex_distance_ratio = 0.01f;
	uint32_t path_length = 0;

	ReSTIRGIConfig* config;
};
