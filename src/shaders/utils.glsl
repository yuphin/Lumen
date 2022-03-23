#ifndef UTILS_DEVICE
#define UTILS_DEVICE
#include "commons.h"
#define PI 3.14159265359
#define PI2 6.28318530718
#define INF 1e10
#define EPS 0.001
#define sqrt2 1.41421356237309504880

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
    vec3 metalness;
    float roughness;
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
    return uint((p.x * 73856093) ^ (p.y * 19349663) ^ p.z * 83492791) %
           (10 * size);
    // return uint(p.x + p.y * grid_res.x + p.z * grid_res.x * grid_res.y);
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
    return vec3(p.x + float_scale * n.x, p.y + float_scale * n.y,
                p.z + float_scale * n.z);
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

// Creates a quaternion s.t the unit vector v becomes (0,0,1)
vec4 to_local_quat(vec3 v) {
    if (v.z < -0.99999f)
        return vec4(1, 0, 0, 0);
    return normalize(vec4(v.y, -v.x, 0.0f, 1.0f + v.z));
}

// Calculates the rotation quaternion w.r.t (0,0,1)
vec4 in_local_quat(vec3 v) {
      if (v.z < -0.99999f)
        return vec4(1, 0, 0, 0);
    return normalize(vec4(-v.y, v.x, 0.0f, 1.0f + v.z));
}

vec4 invert_quat(vec4 q)
{
	return vec4(-q.x, -q.y, -q.z, q.w);
}

vec3 rot_quat(vec4 q, vec3 v) {
    const vec3 q_axis = vec3(q.x, q.y, q.z);
    return 2.0f * dot(q_axis, v) * q_axis +
           (q.w * q.w - dot(q_axis, q_axis)) * v +
           2.0f * q.w * cross(q_axis, v);
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

vec3 sample_cos_hemisphere(vec2 uv, vec3 n, inout float phi) {
    phi = PI2 * uv.x;
    float cos_theta = 2.0 * uv.y - 1.0;
    return normalize(
        n + vec3(sqrt(1.0 - cos_theta * cos_theta) * vec2(cos(phi), sin(phi)),
                 cos_theta));
}

vec3 fresnel_schlick(vec3 f0, float ns) {
    return f0 + (1 - f0) * pow(1.0f - ns, 5.0f);
}

float beckmann_alpha_to_s(float alpha) {
	return 2.0f / min(0.9999f, max(0.0002f, (alpha * alpha))) - 2.0f;
}

vec3 sample_phong(vec2 uv, const vec3 v, const vec3 n, float s) {
    // Transform into local space where the n = (0,0,1)
    vec4 local_quat = to_local_quat(n);
    vec3 v_loc = rot_quat(local_quat, v);
    float phi = PI2 * uv.x;
    float cos_theta = pow(1. - uv.x, 1. / (1. + s));
    float sin_theta = sqrt(1 - cos_theta * cos_theta);
    vec3 local_phong_dir = vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
    vec3 reflected_local_dir = reflect(v_loc, vec3(0, 0, 1));
    vec3 local_phong_dir_rotated = rot_quat(in_local_quat(reflected_local_dir), local_phong_dir);
    return normalize(rot_quat(invert_quat(local_quat), local_phong_dir_rotated));
}

// vec3 shadowedf90(vec3 f0) {
//     const float t = (1. / 0.04);
//     return min(1., t * luminance(f0));
// }

float beckmann_d(float alpha, float nh) {
    nh = max(0.00001f, nh);
    alpha = max(0.00001f, alpha);
    float alpha2 = alpha * alpha;
    float cos2 = nh * nh;
    float num = exp((cos2 - 1) / (alpha2 * cos2));
    float denom = PI * alpha2 * cos2 * cos2;
    return num / denom;
}

vec3 sample_beckmann(vec2 uv, float alpha, vec3 n, const vec3 v) {
     // Transform into local space where the n = (0,0,1)
    vec4 local_quat = to_local_quat(n);
    vec3 v_loc = rot_quat(local_quat, v);
    float tan2 = -(alpha * alpha) * log(1. - uv.x);
    float phi = PI2 * uv.y;
    float cos_theta = 1./ (sqrt(1. + tan2));
    float sin_theta = sqrt(1 - cos_theta * cos_theta);
    const vec3 h = normalize(vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta));
    const vec3 l = reflect(-v_loc, h);
    return normalize(rot_quat(invert_quat(local_quat), l));
}

void correct_shading_normal(const vec3 wo, const vec3 wi,
                            inout vec3 shading_nrm, inout vec3 geometry_nrm) {
    float res1 = abs(dot(wo, shading_nrm)) * abs(dot(wi, geometry_nrm));
    float res2 = abs(dot(wo, geometry_nrm)) * abs(dot(wi, shading_nrm));
    if (res1 != res2) {
        shading_nrm *= -1;
    }
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

#endif