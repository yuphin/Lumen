#define PI 3.14159265359
#define PI2 6.28318530718
#include "commons.h"
struct HitPayload {
    vec3 hit_value;
    vec3 geometry_nrm;
    vec3 shading_nrm;
    vec3 pos;
    vec2 uv;
    uint material_idx;
    float area;
};

struct AnyHitPayload {
    int hit;
};

struct TriangleRecord {
    vec3 pos;
    vec3 triangle_normal;
    float triangle_pdf;
};

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

    return vec3(abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
                abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
                abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z);
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