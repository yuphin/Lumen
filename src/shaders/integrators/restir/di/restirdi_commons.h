#include "../../../commons.h"

struct PCReSTIR {
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
	int enable_accumulation;
	uint light_candidate_count;
	uint enable_gi;
};

struct RestirData {
	LightSampleIdentity identity;
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
