#include "../../commons.h"
#ifndef VCM_COMMONS_H
#define VCM_COMMONS_H
struct PCVCM {
	vec3 sky_col;
	uint frame_num;
	vec3 min_bounds;
	uint size_x;
	vec3 max_bounds;
	uint size_y;
	ivec3 grid_res;
	int num_lights;
	uint time;
	int max_depth;
	float total_light_area;
	int light_triangle_count;
	uint dir_light_idx;
	float radius;
	int use_vm;
	int use_vc;
	uint do_spatiotemporal;
	uint random_num;
	uint max_angle_samples;
	uint total_frame_num;
};


struct VCMRestirData {
	uvec4 seed;
	vec3 pos;
	float p_hat;
	vec3 dir;
	float pdf_posdir;
	vec3 normal;
	float pdf_pos;
	float pdf_dir;
	float triangle_pdf;
	uint light_material_idx;
	uint hash_idx;
	uint valid;
	uint frame_idx;
	float phi;
	uint pad;
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
	float triangle_pdf;
	vec3 dir;
	uint hash_idx;
	vec3 normal;
	vec3 Le;
	uint light_flags;
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