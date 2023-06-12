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
};


struct GBuffer {
	vec3 pos;
	vec3 n_g;
	vec3 n_s;
	vec2 uv;
	uint material_idx;
};

struct GrisData {
	vec3 F; // Integrand
	uvec4 init_seed;
	uvec4 rc_seed;
	vec3 rc_pos;
	uint postfix_length;
	vec2 rc_uv;
	uint prefix_length;
	uint rc_side;
	vec3 rc_ns;
	float rc_g;
	vec3 rc_wi;
	uint rc_mat_id;
	vec3 rc_postfix_L;
	uint rc_nee_visible;
	vec3 rc_nee_L;
	float m_i;
	vec3 reservoir_contribution;
	vec3 prefix_contribution;
};

struct Reservoir {
	GrisData gris_data;
	uint M;
	float W;
	float w_sum;
};

NAMESPACE_END()