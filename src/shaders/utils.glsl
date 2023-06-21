#ifndef UTILS_DEVICE
#define UTILS_DEVICE
#include "commons.h"
#define PI 3.14159265359
#define PI2 6.28318530718
#define INV_PI (1. / PI)
#define INF 1e10
#define EPS 0.001
#define SHADOW_EPS 2 / 65536.
#define sqrt2 1.41421356237309504880

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
    uint instance_idx;
    vec2 barycentrics;
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
    uint material_idx;
    uint light_idx;
    uint triangle_idx; 
    uint flags;
};

#define pow5(x) (x * x) * (x * x) * x
#define sqr(x) (x * x)

uint hash(ivec3 p, uint size) {
    return uint((p.x * 73856093) ^ (p.y * 19349663) ^ p.z * 83492791) %
           (10 * size);
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
    vec3 p_i = vec3(
        intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
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

vec2 sign_not_zero(vec2 v) {
    return vec2(sign_not_zero(v.x), sign_not_zero(v.y));

}

vec2 oct_encode(in vec3 v) {
    float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
    vec2 result = v.xy * (1.0 / l1norm);
    if (v.z < 0.0)
        result = (1.0 - abs(result.yx)) * sign_not_zero(result.xy);
    return result;
}

vec3 oct_decode(vec2 o) {
    vec3 v = vec3(o.x, o.y, 1.0 - abs(o.x) - abs(o.y));
    if (v.z < 0.0)
        v.xy = (1.0 - abs(v.yx)) * sign_not_zero(v.xy);

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
float uint_to_float(uint x) {
    return uintBitsToFloat(0x3f800000 | (x >> 9)) - 1.0f;
}

uvec4 init_rng(uvec2 pixel_coords, uvec2 resolution, uint frame_num, uint state) {
    return uvec4(pixel_coords.xy, frame_num, 0);
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

vec4 invert_quat(vec4 q) { return vec4(-q.x, -q.y, -q.z, q.w); }

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
    vec3 local_phong_dir =
        vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
    vec3 reflected_local_dir = reflect(v_loc, vec3(0, 0, 1));
    vec3 local_phong_dir_rotated =
        rot_quat(in_local_quat(reflected_local_dir), local_phong_dir);
    return normalize(
        rot_quat(invert_quat(local_quat), local_phong_dir_rotated));
}

float beckmann_d(float alpha, float nh) {
    nh = max(0.00001f, nh);
    alpha = max(0.00001f, alpha);
    float alpha2 = alpha * alpha;
    float cos2 = nh * nh;
    float num = exp((cos2 - 1) / (alpha2 * cos2));
    float denom = PI * alpha2 * cos2 * cos2;
    return num / denom;
}

float schlick_w(float u) {
    float m = clamp(1 - u, 0, 1);
    float m2 = m * m;
    return m2 * m2 * m;
}

float GTR1(float nh, float a) {
    if (a >= 1) {
        return 1 / PI;
    }
    float a2 = a * a;
    float t = 1 + (a2 - 1) * nh * nh;
    return (a2 - 1) / (PI * log(a2) * t);
}

float GTR2(float nh, float a) {
    float a2 = a * a;
    float t = 1 + (a2 - 1) * nh * nh;
    return a2 / (PI * t * t);
}

float GTR2_aniso(float nh, float hx, float hy, float ax, float ay) {
    return 1 / (PI * ax * ay * sqr(sqr(hx / ax) + sqr(hy / ay) + nh * nh));
}

float smithG_GGX(float nv, float alpha_g) {
    float a = alpha_g * alpha_g;
    float b = nv * nv;
    return 1 / (nv + sqrt(a + b - a * b));
}

float smithG_GGX_aniso(float nv, float vx, float vy, float ax, float ay) {
    return 1 / (nv + sqrt(sqr(vx * ax) + sqr(vy * ay) + sqr(nv)));
}

// Input Ve: view direction
// Input alpha_x, alpha_y: roughness parameters
// Input U1, U2: uniform random numbers
// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) *
// D(Ne) / Ve.z
vec3 sample_ggx_vndf(vec3 Ve, float alpha_x, float alpha_y, float U1, float U2,
                     const vec3 n) {
    // World to local
    vec4 local_quat = to_local_quat(n);
    Ve = rot_quat(local_quat, Ve);

    // Section 3.2: transforming the view direction to the hemisphere
    // configuration
    vec3 Vh = normalize(vec3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
    // Section 4.1: orthonormal basis (with special case if cross product is
    // zero)
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    vec3 T1 =
        lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1, 0, 0);
    vec3 T2 = cross(Vh, T1);
    // Section 4.2: parameterization of the projected area
    float r = sqrt(U1);
    float phi = 2.0 * PI * U2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
    // Section 4.3: reprojection onto hemisphere
    vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    // Section 3.4: transforming the normal back to the ellipsoid configuration
    const vec3 h =
        normalize(vec3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));
    const vec3 l = reflect(-Ve, h);
    // Local to world
    return normalize(rot_quat(invert_quat(local_quat), l));
}

vec3 sample_beckmann(vec2 uv, float alpha, vec3 n, const vec3 v) {
    // Transform into local space where the n = (0,0,1)
    vec4 local_quat = to_local_quat(n);
    vec3 v_loc = rot_quat(local_quat, v);
    float tan2 = -(alpha * alpha) * log(1. - uv.x);
    float phi = PI2 * uv.y;
    float cos_theta = 1. / (sqrt(1. + tan2));
    float sin_theta = sqrt(1 - cos_theta * cos_theta);
    const vec3 h =
        normalize(vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta));
    const vec3 l = reflect(-v_loc, h);
    return normalize(rot_quat(invert_quat(local_quat), l));
}

vec3 sample_gtr1(vec2 uv, float alpha2, const vec3 n, vec3 v) {
    vec4 local_quat = to_local_quat(n);
    v = rot_quat(local_quat, v);
    const float cos_theta =
        sqrt(max(0, (1. - pow(alpha2, 1.0 - uv.x)) / (1.0 - alpha2)));
    const float sin_theta = sqrt(max(0, 1. - cos_theta * cos_theta));
    const float phi = PI2 * uv.y;
    const vec3 h = vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
    const vec3 l = reflect(-v, h);
    return normalize(rot_quat(invert_quat(local_quat), l));
}

void correct_shading_normal(const vec3 wo, const vec3 wi, inout vec3 n_s,
                            inout vec3 n_g) {
    float res1 = abs(dot(wo, n_s)) * abs(dot(wi, n_g));
    float res2 = abs(dot(wo, n_g)) * abs(dot(wi, n_s));
    if (res1 != res2) {
        n_s *= -1;
    }
}

void make_coord_system(const vec3 v1, out vec3 v2, out vec3 v3) {
    if (abs(v1.x) > abs(v1.y)) {
        v2 = normalize(vec3(-v1.z, 0, v1.x));
    } else {
        v2 = normalize(vec3(0, v1.z, -v1.y));
    }
    v3 = cross(v1, v2);
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

#endif