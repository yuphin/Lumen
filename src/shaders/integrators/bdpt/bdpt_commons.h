#ifndef BDPT_COMMONS_H
#define BDPT_COMMONS_H
#include "../../commons.h"

struct PCBDPT {
	vec3 sky_col;
	uint frame_num;
	uint width;
	uint height;
	int num_lights;
	uint time;
	int max_depth;
	uint dir_light_idx;
	uint enable_accumulation;
};

struct PathVertex {
	vec3 dir;
	vec3 n_s;
	vec3 pos;
	vec2 uv;
	vec3 throughput;
	uint light_flags;
	uint light_idx;
	uint material_idx;
	uint delta;
	uint side;
	uint mode;
	float area;
	float pdf_fwd;
	float pdf_rev;
};
#endif
