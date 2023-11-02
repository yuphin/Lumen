#include "../../../commons.h"

NAMESPACE_BEGIN(RestirPT)

struct PCReSTIRPT {
	vec3 sky_col;
	uint frame_num;
	uint size_x;
	uint size_y;
	int num_lights;
	uint time;
	int max_depth;
	float total_light_area;
	int light_triangle_count;
	uint dir_light_idx;
	uint random_num;
	uint prev_random_num;
	uint total_frame_num;
	uint enable_accumulation;
	float scene_extent;
	uint num_spatial_samples;
	uint direct_lighting;
	uint enable_rr;
	uint enable_spatial_reuse;
	uint show_reconnection_radiance;
	float spatial_radius;
	float min_vertex_distance_ratio;
	uint path_length;
	uint buffer_idx;
	uint enable_gris;
};


struct GBuffer {
	uvec2 pad1;
	vec2 barycentrics;
	uvec2 primitive_instance_id;
	uvec2 pad2;
};

struct GrisData {
	uvec4 debug_seed;
	vec3 rc_wi;
	uint init_seed;
	vec3 rc_Li;
	// Layout for the path flags
	// | 5b postfix_length| 5b prefix_length |2b is_nee/is_emissive_after_rc/default
	uint path_flags;
	vec3 reservoir_contribution;
	uint rc_coords;
	vec2 rc_barycentrics;
	uvec2 rc_primitive_instance_id;
	vec2 pad;
	float pdf_light_w;
	uint rc_seed;
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
	vec3 pad;
	float target_pdf_in_neighbor;
};



struct GrisHitPayload {
	vec2 attribs;
	uint instance_idx;
	uint triangle_idx;
	float dist;
};

NAMESPACE_END()