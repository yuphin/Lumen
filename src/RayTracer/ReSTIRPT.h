#pragma once
#include "Framework/AccelerationStructure.h"
#include "Framework/Texture.h"
#include "Framework/Base/SmallArray.h"
#include "shaders/integrators/restir/gris/gris_commons.h"

struct Integrator;

struct ReSTIRPT {
	enum StreamingMethod { STREAM_INDIVIDUAL_CONTRIBUTIONS, STREAM_SPLITTING_AT_RECONNECTION };
	enum MISMethod { MIS_TALBOT, MIS_PAIRWISE };

	vk::Buffer* gris_gbuffer = nullptr;
	vk::Buffer* gris_prev_gbuffer = nullptr;
	vk::Buffer* gris_reservoir_ping_buffer = nullptr;
	vk::Buffer* gris_reservoir_pong_buffer = nullptr;
	vk::Buffer* prefix_contribution_buffer = nullptr;
	vk::Buffer* reconnection_buffer = nullptr;
	vk::Buffer* transformations_buffer = nullptr;
	vk::Buffer* debug_vis_buffer = nullptr;
	vk::Buffer* photon_eye_buffer_ping = nullptr;
	vk::Buffer* photon_eye_buffer_pong = nullptr;
	vk::Buffer* caustic_photon_aabbs_buffer = nullptr;
	vk::Buffer* caustic_photon_light_buffer = nullptr;
	vk::Buffer* photon_count_buffer = nullptr;
	vk::Texture* canonical_contributions_texture = nullptr;
	vk::Texture* direct_lighting_texture = nullptr;
	vk::Texture* caustics_texture = nullptr;
	vk::Buffer* photon_bvh_instances_buf = nullptr;
	lm::SmallArray<vk::Buffer*, vk::MAX_FRAMES_IN_FLIGHT> photon_bvh_scratch_bufs;
	vk::Buffer* caustics_reservoir_ping_buffer = nullptr;
	vk::Buffer* caustics_reservoir_pong_buffer = nullptr;
	RestirPT::PCReSTIRPT pc_ray{};
	bool enable_accumulation = true;
	bool direct_lighting = true;
	bool enable_rr = false;
	bool enable_spatial_reuse = true;
	bool canonical_only = false;
	bool hide_reconnection_radiance = false;
	bool enable_temporal_reuse = true;
	bool enable_pm_temporal_reuse = true;
	bool enable_gris = false;
	bool pixel_debug = false;
	bool enable_permutation_sampling = false;
	bool enable_atmosphere = false;
	bool enable_defensive_formulation = true;
	bool enable_occlusion = true;
	bool enable_temporal_jitter = true;
	bool enable_photon_mapping = true;
	bool enable_photon_gather = true;
	bool progressive_radius_reduction = false;
	bool enable_pm_mis = false;
	f32 spatial_reuse_radius = 32.0f;
	f32 min_vertex_distance_ratio = 0.00f;
	f32 gris_separator = 1.0f;
	f32 initial_photon_radius = 0.03f;
	f32 curr_photon_radius = initial_photon_radius;
	u32 path_length = 0;
	u32 num_spatial_samples = 1;
	u32 num_photons = 1920 * 1080;
	StreamingMethod streaming_method = STREAM_INDIVIDUAL_CONTRIBUTIONS;
	MISMethod mis_method = MIS_PAIRWISE;
	vk::BlasInput photon_blas_input;
	vk::BVH photon_blas;
	vk::BVH photon_tlas;
};

namespace restirpt {
void init(Integrator* integrator);
void render(Integrator* integrator);
bool update(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
bool gui(Integrator* integrator);
}  // namespace restirpt
