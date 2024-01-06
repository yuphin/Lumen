#ifndef UTILS_DEVICE
#define UTILS_DEVICE
#include "commons.h"
#define PI 3.14159265359
#define TWO_PI 6.28318530718
#define INV_PI (1. / PI)
#define INF 1e10
#define EPS 0.001
#define SHADOW_EPS 2 / 65536.
#define sqrt2 1.41421356237309504880

#define TRANSPORT_MODE_FROM_CAMERA 1
#define TRANSPORT_MODE_FROM_LIGHT 0

struct HitPayload {
	vec3 n_g;
	vec3 n_s;
	vec3 pos;
	vec2 uv;
	uint material_idx;
	uint triangle_idx;
	float area;
	float dist;
	uint hit_kind;
};

struct VCMState {
	vec3 wi;
	vec3 n_s;
	vec3 pos;
	vec2 uv;
	vec3 throughput;
	uint material_idx;
	float area;
	float d_vcm;
	float d_vc;
	float d_vm;
};

struct AnyHitPayload {
	int hit;
};

struct TriangleRecord {
	vec3 pos;
	vec3 n_s;
	float triangle_pdf;
};

struct LightRecord {
	uint light_idx;
	uint flags;
	vec2 bary;
	uint triangle_idx;
	float triangle_area;
};

#define pow5(x) (((x) * (x)) * ((x) * (x)) * (x))
#define sqr(x) (x * x)

uint hash(ivec3 p, uint size) {
	return uint((p.x * 73856093) ^ (p.y * 19349663) ^ p.z * 83492791) % (10 * size);
	// return uint(p.x + p.y * grid_res.x + p.z * grid_res.x * grid_res.y);
}
#define FLT_EPSILON 0.5 * 1.19209290E-07
float gamma(int n) {
#define MachineEpsilon
	return (n * FLT_EPSILON) / (1 - n * FLT_EPSILON);
}

// Ray Tracing Gems chapter 6
vec3 offset_ray(const vec3 p, const vec3 n) {
	const float origin = 1.0f / 32.0f;
	const float float_scale = 1.0f / 65536.0f;
	const float int_scale = 256.0f;
	ivec3 of_i = ivec3(int_scale * n.x, int_scale * n.y, int_scale * n.z);
	vec3 p_i = vec3(intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
					intBitsToFloat(floatBitsToInt(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
					intBitsToFloat(floatBitsToInt(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));
	return vec3(abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
				abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
				abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
}

vec3 offset_ray2(const vec3 p, const vec3 n) {
	const float float_scale = 2.0f / 65536.0f;
	return p + float_scale * n;
}

float luminance(vec3 rgb) { return dot(rgb, vec3(0.2126f, 0.7152f, 0.0722f)); }

float sign_not_zero(float k) { return (k >= 0.0) ? 1.0 : -1.0; }

vec2 sign_not_zero(vec2 v) { return vec2(sign_not_zero(v.x), sign_not_zero(v.y)); }

vec2 oct_encode(in vec3 v) {
	float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
	vec2 result = v.xy * (1.0 / l1norm);
	if (v.z < 0.0) result = (1.0 - abs(result.yx)) * sign_not_zero(result.xy);
	return result;
}

vec3 oct_decode(vec2 o) {
	vec3 v = vec3(o.x, o.y, 1.0 - abs(o.x) - abs(o.y));
	if (v.z < 0.0) v.xy = (1.0 - abs(v.yx)) * sign_not_zero(v.xy);

	return normalize(v);
}

// PCG random numbers generator
// Source: "Hash Functions for GPU Rendering" by Jarzynski & Olano
uvec4 pcg4d(uvec4 v) {
	v = v * 1664525u + 1013904223u;
	v.x += v.y * v.w;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v.w += v.y * v.z;
	v = v ^ (v >> 16u);
	v.x += v.y * v.w;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v.w += v.y * v.z;
	return v;
}

// Returns a float between 0 and 1
float uint_to_float(uint x) { return uintBitsToFloat(0x3f800000 | (x >> 9)) - 1.0f; }

uvec4 init_rng(uvec2 pixel_coords, uvec2 resolution, uint frame_num, uint state) {
	return uvec4(pixel_coords.xy, frame_num, state);
}

uvec4 init_rng(uvec2 pixel_coords, uvec2 resolution, uint frame_num) { return uvec4(pixel_coords.xy, frame_num, 0); }

// Return random float in (0, 1) range
float rand(inout uvec4 rng_state) {
	rng_state.w++;
	return uint_to_float(pcg4d(rng_state).x);
}

vec2 rand2(inout uvec4 rng_state) { return vec2(rand(rng_state), rand(rng_state)); }

vec3 rand3(inout uvec4 rng_state) { return vec3(rand2(rng_state), rand(rng_state)); }

vec4 rand4(inout uvec4 rng_state) { return vec4(rand3(rng_state), rand(rng_state)); }

// Creates a quaternion s.t the unit vector v becomes (0,0,1)
vec4 to_local_quat(vec3 v) {
	if (v.z < -0.99999f) return vec4(1, 0, 0, 0);
	return normalize(vec4(v.y, -v.x, 0.0f, 1.0f + v.z));
}

// Calculates the rotation quaternion w.r.t (0,0,1)
vec4 in_local_quat(vec3 v) {
	if (v.z < -0.99999f) return vec4(1, 0, 0, 0);
	return normalize(vec4(-v.y, v.x, 0.0f, 1.0f + v.z));
}

vec4 invert_quat(vec4 q) { return vec4(-q.x, -q.y, -q.z, q.w); }

vec3 rot_quat(vec4 q, vec3 v) {
	const vec3 q_axis = vec3(q.x, q.y, q.z);
	return 2.0f * dot(q_axis, v) * q_axis + (q.w * q.w - dot(q_axis, q_axis)) * v + 2.0f * q.w * cross(q_axis, v);
}

void make_coord_system(const vec3 v1, out vec3 v2, out vec3 v3) {
	if (abs(v1.x) > abs(v1.y)) {
		v2 = normalize(vec3(-v1.z, 0, v1.x));
	} else {
		v2 = normalize(vec3(0, v1.z, -v1.y));
	}
	v3 = cross(v1, v2);
}

// https://graphics.pixar.com/library/OrthonormalB/paper.pdf
void branchless_onb(vec3 n, out vec3 b1, out vec3 b2) {
	float sign = n.z >= 0.0 ? 1.0 : -1.0;
	float a = -1.0 / (sign + n.z);
	float b = n.x * n.y * a;
	b1 = vec3(1.0 + sign * n.x * n.x * a, sign * b, -sign * n.x);
	b2 = vec3(b, sign + n.y * n.y * a, -n.y);
}

vec3 to_world(vec3 v, vec3 T, vec3 B, vec3 N) { return v.x * T + v.y * B + v.z * N; }

vec3 to_local(vec3 v, vec3 T, vec3 B, vec3 N) { return vec3(dot(v, T), dot(v, B), dot(v, N)); }

bool face_forward(inout vec3 n_s, inout vec3 n_g, vec3 wo) {
	bool side = true;
	if (dot(n_g, wo) < 0.0) {
		n_g *= -1;
	}
	if (dot(n_g, n_s) < 0.0) {
		n_s *= -1;
		side = false;
	}
	return side;
}

void face_forward(inout vec3 n_s, bool side) {
	if (!side) {
		n_s *= -1;
	}
}

vec2 concentric_sample_disk(vec2 rands) {
	vec2 offset = 2 * rands - 1;
	if (offset.x == 0 && offset.y == 0) {
		return vec2(0);
	}
	float theta, r;
	if (abs(offset.x) > abs(offset.y)) {
		r = offset.x;
		theta = 0.25 * PI * offset.y / offset.x;
	} else {
		r = offset.y;
		theta = PI * (0.5 - 0.25 * offset.x / offset.y);
	}
	return r * vec2(cos(theta), sin(theta));
}

// From PBRT
float erf_inv(float x) {
	float w, p;
	x = clamp(x, -.99999, .99999);
	w = -log((1 - x) * (1 + x));
	if (w < 5) {
		w = w - 2.5;
		p = 2.81022636e-08;
		p = 3.43273939e-07 + p * w;
		p = -3.5233877e-06 + p * w;
		p = -4.39150654e-06 + p * w;
		p = 0.00021858087 + p * w;
		p = -0.00125372503 + p * w;
		p = -0.00417768164 + p * w;
		p = 0.246640727 + p * w;
		p = 1.50140941 + p * w;
	} else {
		w = sqrt(w) - 3;
		p = -0.000200214257;
		p = 0.000100950558 + p * w;
		p = 0.00134934322 + p * w;
		p = -0.00367342844 + p * w;
		p = 0.00573950773 + p * w;
		p = -0.0076224613 + p * w;
		p = 0.00943887047 + p * w;
		p = 1.00167406 + p * w;
		p = 2.83297682 + p * w;
	}
	return p * x;
}
bool bsdf_has_property(uint props, uint flag) { return (props & flag) != 0; }

#endif