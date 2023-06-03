#include "../../../commons.h"

struct PCReSTIRGI {
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
	uint total_frame_num;
	float world_radius;
	int enable_accumulation;
};

struct ReservoirSample {
	vec3 x_v;
	float p_q;
	vec3 n_v;
	uint bsdf_props;
	vec3 x_s;
	uint mat_idx;
	vec3 n_s;
	vec3 L_o;
	vec3 f;
};

struct Reservoir {
	float w_sum;
	float W;
	uint m;
	uint pad;
	ReservoirSample s;
};