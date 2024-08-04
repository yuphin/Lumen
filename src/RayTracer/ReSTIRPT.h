#pragma once
#include "Framework/Texture.h"
#include "Integrator.h"
#include "shaders/integrators/restir/gris/gris_commons.h"
using namespace RestirPT;
class ReSTIRPT : public Integrator {
   public:
	ReSTIRPT(lumen::LumenInstance* scene, LumenScene* lumen_scene)
		: Integrator(scene, lumen_scene), config(CAST_CONFIG(lumen_scene->config.get(), ReSTIRPTConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy() override;
	virtual bool gui() override;

   private:
	enum class StreamingMethod { INDIVIDUAL_CONTRIBUTIONS, SPLITTING_AT_RECONNECTION };

	enum class MISMethod { TALBOT, PAIRWISE };
	lumen::BufferOld direct_lighting_buffer;
	lumen::BufferOld gris_gbuffer;
	lumen::BufferOld gris_prev_gbuffer;
	lumen::BufferOld gris_reservoir_ping_buffer;
	lumen::BufferOld gris_reservoir_pong_buffer;
	lumen::BufferOld prefix_contribution_buffer;
	lumen::BufferOld reconnection_buffer;
	lumen::BufferOld transformations_buffer;
	lumen::BufferOld debug_vis_buffer;
	lumen::Texture2D canonical_contributions_texture;
	PCReSTIRPT pc_ray{};
	bool enable_accumulation = false;
	bool direct_lighting = false;
	bool enable_rr = false;
	bool enable_spatial_reuse = true;
	bool canonical_only = false;
	bool show_reconnection_radiance = false;
	bool enable_temporal_reuse = true;
	bool enable_gris = true;
	bool pixel_debug = false;
	bool enable_permutation_sampling = false;
	bool enable_atmosphere = false;
	bool enable_defensive_formulation = true;
	bool enable_occlusion = true;
	float spatial_reuse_radius = 32.0f;
	float min_vertex_distance_ratio = 0.00f;
	float gris_separator = 1.0f;
	uint32_t path_length = 0;
	uint32_t num_spatial_samples = 1;
	StreamingMethod streaming_method = StreamingMethod::INDIVIDUAL_CONTRIBUTIONS;
	MISMethod mis_method = MISMethod::PAIRWISE;
	ReSTIRPTConfig* config;
};
