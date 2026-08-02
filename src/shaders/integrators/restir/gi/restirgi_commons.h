#include "../../../commons.h"

struct PCReSTIRGI {
	vec3 sky_col;
	uint frame_num;
	uint width;
	uint height;
	int num_lights;
	uint time;
	int max_depth;
	uint dir_light_idx;
	uint do_spatiotemporal;
	uint random_num;
	uint total_frame_num;
	float world_radius;
	int enable_accumulation;
	int enable_di;
};

struct ReservoirSample {
	SurfaceRef x_v_surface;
	SurfaceRef x_s_surface;
	vec3 L_o;
	float p_q;
	vec3 f;
	uint bsdf_props;
};

struct Reservoir {
	float w_sum;
	float W;
	uint m;
	uint pad;
	ReservoirSample s;
};

#ifdef __cplusplus
static_assert(sizeof(ReservoirSample) == 64);
static_assert(sizeof(Reservoir) == 80);
#endif
