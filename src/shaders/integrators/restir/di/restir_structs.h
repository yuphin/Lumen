#pragma once
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