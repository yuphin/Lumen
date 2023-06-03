#include "../../../commons.h"

struct PCReSTIR {
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
	uint do_spatiotemporal;
	uint random_num;
	int enable_accumulation;
};

struct RestirData {
	uint light_idx;
	uint light_mesh_idx;
	uvec4 seed;
};

struct RestirReservoir {
	float w_sum;
	float W;
	uint m;
	RestirData s;
	float p_hat;
	float pdf;
};

struct RestirGBufferData {
	vec3 pos;
	uint mat_idx;
	vec3 normal;
	uint pad;
	vec2 uv;
	vec2 pad2;
	vec3 albedo;
	uint pad3;
	vec3 n_g;
	uint pad4;
};