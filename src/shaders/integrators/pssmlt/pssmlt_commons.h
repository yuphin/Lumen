#include "../mlt_commons.h"
#include "../bdpt/bdpt_commons.h"

struct MLTPathVertex {
	vec3 dir;
	uint delta;
	vec3 n_s;
	float area;
	vec3 pos;
	uint light_idx;
	vec2 uv;
	uint light_flags;
	uint material_idx;
	vec3 throughput;
	float pdf_rev;
	uint coords;
	float pdf_fwd;
	vec2 pad;
};