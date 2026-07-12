#include "../../commons.h"
#ifndef VCM_COMMONS_H
#define VCM_COMMONS_H
struct PCVCM {
	vec3 sky_col;
	uint frame_num;
	vec3 min_bounds;
	uint width;
	vec3 max_bounds;
	uint height;
	ivec3 grid_res;
	int num_lights;
	uint time;
	int max_depth;
	uint dir_light_idx;
	float radius;
	int use_vm;
	int use_vc;
	uint do_spatiotemporal;
	uint random_num;
	uint max_angle_samples;
	uint total_frame_num;
	uint enable_accumulation;
};


struct VCMRestirData {
	float p_hat;
	vec3 dir;
	float pdf_dir;
	uint hash_idx;
	uint valid;
	uint frame_idx;
};

struct VCMReservoir {
	float w_sum;
	float W;
	uint m;
	uint sample_idx;
	uint selected_idx;
	uint factor;
	VCMRestirData s;
};

struct VCMPhotonHash {
	vec3 pos;
	float d_vm;
	vec3 wi;
	float d_vcm;
	vec3 throughput;
	int photon_count;
	vec3 nrm;
	uint path_len;
};

struct AngleStruct {
	float phi;
	float theta;
	int is_active;
};

struct LightState {
	vec3 pos;
	float pdf_position_a;
	vec3 dir;
	uint hash_idx;
	vec3 normal;
	float pdf_direction_w;
	vec3 Le;
	uint light_flags;
	float cos_from_light;
	float area;
};

struct SelectedReservoirs {
	uint selected;
	vec3 pos;
	vec3 dir;
};

struct VCMVertex {
	vec3 wi;
	vec3 wo;
	vec3 n_s;
	vec3 pos;
	vec2 uv;
	vec3 throughput;
	uint material_idx;
	uint path_len;
	float area;
	float d_vcm;
	float d_vc;
	float d_vm;
	uint side;
	uint coords;
};

struct AvgStruct {
	float avg;
	uint prev;
};
#endif
