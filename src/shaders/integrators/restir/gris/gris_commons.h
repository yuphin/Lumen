#include "../../../commons.h"

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
	float max_spatial_radius;
	float scene_extent;
	uint num_spatial_samples;
};


struct ReSTIRPTGBuffer {
	vec3 pos;
	vec3 n_g;
	vec3 n_s;
	vec2 uv;
	uint material_idx;
};

struct GrisData {
	vec3 F; // Integrand
	uvec4 init_seed;
	// Reconnection vertex data
	uvec4 rc_seed;
	vec3 rc_pos;
	vec2 rc_uv;
	vec3 rc_ns;
	vec3 rc_wi;
	uint rc_mat_id;
	vec3 rc_postfix_L;
	uint rc_side;
	uint rc_nee_visible;
	vec3 rc_nee_L;
	uint postfix_length;
	uint rc_depth;
};

struct ReSTIRPTReservoir {
	GrisData gris_data;
	uint M;
	float W;
	float w_sum;
};