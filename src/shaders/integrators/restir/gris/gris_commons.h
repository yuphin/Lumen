// #define DISABLE_LOGGING
#include "../../../commons.h"

#define DEBUG 1

NAMESPACE_BEGIN(RestirPT)

struct PCReSTIRPT {
	vec3 sky_col;
	uint frame_num;
	uint width;
	uint height;
	int num_lights;
	uint time;
	int max_depth;
	float total_light_area;
	int total_light_count;
	uint dir_light_idx;
	uint general_seed;
	uint sampling_seed;
	uint seed2;
	uint seed3;
	uint prev_random_num;
	uint total_frame_num;
	uint enable_accumulation;
	float scene_extent;
	uint num_spatial_samples;
	uint direct_lighting;
	uint enable_rr;
	uint enable_spatial_reuse;
	uint hide_reconnection_radiance;
	float spatial_radius;
	float min_vertex_distance_ratio;
	uint path_length;
	uint buffer_idx;
	uint enable_gris;
	uint temporal_reuse;
	uint pixel_debug;
	uint permutation_sampling;
	uint canonical_only;
	float gris_separator;
	uint enable_occlusion;
	uint enable_temporal_jitter;
	float photon_radius;
	uint num_photons;
	uint pm_temporal_reuse;
};

struct GBuffer {
	vec2 barycentrics;
	uvec2 primitive_instance_id;
};

struct GrisData {
#if DEBUG == 1
	uvec4 debug_sampling_seed;
	uvec4 debug_seed;
#endif
	vec3 rc_Li;
	float reservoir_contribution;
	vec2 rc_wi;
	uint rc_seed;
	// Layout for the path flags
	// 1b is_directional_light | 1b side | 5b postfix_length| 5b prefix_length |3b
	// is_nee/is_nee_postfix/emissive_after_rc/emissive/default
	uint path_flags;
	vec2 rc_barycentrics;
	uvec2 seed_helpers;
	uvec2 rc_primitive_instance_id;
	uint rc_coords;
	float rc_partial_jacobian;	// g * rc_pdf (* rc_postfix_pdf)
};

struct Reservoir {
	GrisData data;
	uint M;
	float W;
	float w_sum;
	float target_pdf;
};

struct ReconnectionData {
	vec3 reservoir_contribution;
	float jacobian;
	vec2 pad;
	float new_jacobian;
	float target_pdf_in_neighbor;
};

struct GrisHitPayload {
	vec2 attribs;
	uint instance_idx;
	uint triangle_idx;
	float dist;
};

struct PhotonData {
	vec2 barycentrics;
	uvec2 primitive_instance_id;
	vec3 throughput;
	//  5b eye/light path length | 1b side
	uint flags; 
	vec2 dir;
	float d_vm;
	float pad;
};

struct PhotonAABB {
	vec3 min;
	vec3 max;
};
struct PhotonReservoir {
	vec2 barycentrics;
	uvec2 primitive_instance_id;
	vec3 flux;
	uint M;
	vec2 wi;
	float W;
	float w_sum;
	float target_pdf;
	float d_vm;
	uint flags; 
	float pad;
};

NAMESPACE_END()