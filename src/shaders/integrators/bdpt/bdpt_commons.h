#ifndef BDPT_COMMONS_H
#define BDPT_COMMONS_H
#include "../../commons.h"

struct PCBDPT {
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
	float area;
	float pdf_fwd;
	float pdf_rev;
};
#endif