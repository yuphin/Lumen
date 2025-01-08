#pragma once
#include "Framework/Texture.h"
#include "Integrator.h"
#include "shaders/integrators/restir/gris/gris_commons.h"
using namespace RestirPT;
class ReSTIRPT final : public Integrator {
   public:
	ReSTIRPT(LumenScene* lumen_scene, const vk::BVH& tlas)
		: Integrator(lumen_scene, tlas), config(CAST_CONFIG(lumen_scene->config.get(), ReSTIRPTConfig)) {}
	virtual void init() override;
	virtual void render() override;
	virtual bool update() override;
	virtual void destroy(bool resize) override;
	virtual bool gui() override;

   private:
	enum class StreamingMethod { INDIVIDUAL_CONTRIBUTIONS, SPLITTING_AT_RECONNECTION };

	enum class MISMethod { TALBOT, PAIRWISE };
	vk::Buffer* gris_gbuffer;
	vk::Buffer* gris_prev_gbuffer;
	vk::Buffer* gris_reservoir_ping_buffer;
	vk::Buffer* gris_reservoir_pong_buffer;
	vk::Buffer* prefix_contribution_buffer;
	vk::Buffer* reconnection_buffer;
	vk::Buffer* transformations_buffer;
	vk::Buffer* debug_vis_buffer;
	vk::Buffer* photon_eye_buffer;
	vk::Buffer* caustic_photon_aabbs_buffer;
	vk::Buffer* caustic_photon_light_buffer;
	vk::Buffer* photon_count_buffer;
	vk::Texture* canonical_contributions_texture;
	vk::Texture* direct_lighting_texture;

	vk::Buffer* photon_bvh_scratch_buf = nullptr;
	vk::Buffer* photon_bvh_scratch_buf2 = nullptr;
	vk::Buffer* photon_bvh_instances_buf = nullptr;

	PCReSTIRPT pc_ray{};
	bool enable_accumulation = false;
	bool direct_lighting = false;
	bool enable_rr = false;
	bool enable_spatial_reuse = true;
	bool canonical_only = false;
	bool hide_reconnection_radiance = false;
	bool enable_temporal_reuse = true;
	bool enable_gris = true;
	bool pixel_debug = false;
	bool enable_permutation_sampling = false;
	bool enable_atmosphere = false;
	bool enable_defensive_formulation = true;
	bool enable_occlusion = true;
	bool enable_temporal_jitter = true;
	bool enable_photon_mapping = true;
	float spatial_reuse_radius = 32.0f;
	float min_vertex_distance_ratio = 0.00f;
	float gris_separator = 1.0f;
	float photon_radius = 0.03f;
	uint32_t path_length = 0;
	uint32_t num_spatial_samples = 1;
	uint32_t num_photons = 1000000;
	StreamingMethod streaming_method = StreamingMethod::INDIVIDUAL_CONTRIBUTIONS;
	MISMethod mis_method = MISMethod::PAIRWISE;
	ReSTIRPTConfig* config;

	std::array<vk::BVH, vk::MAX_FRAMES_IN_FLIGHT> photon_blases;
	std::array<vk::BVH, vk::MAX_FRAMES_IN_FLIGHT> photon_tlases;

};
