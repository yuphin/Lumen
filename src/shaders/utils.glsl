#define PI 3.14159265359
#define PI2 6.28318530718
#include "commons.h"
#define INF 1e10
#define EPS 0.001
struct HitPayload {
    vec3 geometry_nrm;
    vec3 shading_nrm;
    vec3 pos;
    vec2 uv;
    uint material_idx;
    uint triangle_idx;
    float area;
};

struct MaterialProps {
    vec3 emissive_factor;
    uint bsdf_type;
    uint bsdf_props;
    vec3 albedo;
    float ior;
};

struct VCMState {
    vec3 wi;
	vec3 shading_nrm;
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
    vec3 triangle_normal;
    float triangle_pdf;
};

uint hash(ivec3 p, uint size) {
  return uint((p.x * 73856093) ^ (p.y * 19349663) ^
              p.z * 83492791) %  (10 * size);
 //return uint(p.x + p.y * grid_res.x + p.z * grid_res.x * grid_res.y);
}

// Ray Tracing Gems chapter 6
vec3 offset_ray(const vec3 p, const vec3 n) {
    const float origin = 1.0f / 32.0f;
    const float float_scale = 1.0f / 65536.0f;
    const float int_scale = 256.0f;
    ivec3 of_i = ivec3(int_scale * n.x, int_scale * n.y, int_scale * n.z);
    vec3 p_i = vec3(
        intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
        intBitsToFloat(floatBitsToInt(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
        intBitsToFloat(floatBitsToInt(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

#if 0
    return vec3(abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
                abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
                abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
#else
  return vec3(p.x + float_scale * n.x ,
              p.y + float_scale * n.y,
              p.z + float_scale * n.z 
              );
#endif
}

float luminance(vec3 rgb) { return dot(rgb, vec3(0.2126f, 0.7152f, 0.0722f)); }

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
float uint_to_float(uint x) {
    return uintBitsToFloat(0x3f800000 | (x >> 9)) - 1.0f;
}

uvec4 init_rng(uvec2 pixel_coords, uvec2 resolution, uint frame_num) {
    return uvec4(pixel_coords.xy, frame_num, 0);
}

// Return random float in (0, 1) range
float rand(inout uvec4 rng_state) {
    rng_state.w++;
    return uint_to_float(pcg4d(rng_state).x);
}

bool same_hemisphere(in vec3 wi, in vec3 wo, in vec3 n) {
    return dot(wi, n) * dot(wo, n) > 0;
}

vec3 sample_cos_hemisphere(vec2 uv, vec3 n) {
    float phi = PI2 * uv.x;
    float cos_theta = 2.0 * uv.y - 1.0;
    return normalize(
        n + vec3(sqrt(1.0 - cos_theta * cos_theta) * vec2(cos(phi), sin(phi)),
                 cos_theta));
}

void correct_shading_normal(const vec3 wo, const vec3 wi,
                            inout vec3 shading_nrm, inout vec3 geometry_nrm) {
    float res1 = abs(dot(wo, shading_nrm)) * abs(dot(wi, geometry_nrm));
    float res2 = abs(dot(wo, geometry_nrm)) * abs(dot(wi, shading_nrm));
    if (res1 != res2) {
        shading_nrm *= -1;
    }
}