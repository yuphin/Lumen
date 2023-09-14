#include "../../../commons.glsl"
layout(location = 0) rayPayloadEXT HitPayload payload;
layout(location = 1) rayPayloadEXT AnyHitPayload any_hit_payload;
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer ColorStorages { vec3 d[]; };

layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GBuffer { RestirGBufferData d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer RestirReservoir_ {
    RestirReservoir d[];
};
const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;
#define RR_MIN_DEPTH 3

ColorStorages tmp_col = ColorStorages(scene_desc.color_storage_addr);
GBuffer gbuffer = GBuffer(scene_desc.g_buffer_addr);
RestirReservoir_ passthrough_reservoirs =
    RestirReservoir_(scene_desc.passthrough_reservoir_addr);
RestirReservoir_ temporal_reservoirs =
    RestirReservoir_(scene_desc.temporal_reservoir_addr);
RestirReservoir_ spatial_reservoirs =
    RestirReservoir_(scene_desc.spatial_reservoir_addr);

vec3 pos;
vec3 normal;
vec2 uv;
uint mat_idx;
vec3 origin;

uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);
uvec4 seed = init_rng(gl_LaunchIDEXT.xy, gl_LaunchSizeEXT.xy,
                      pc_ray.frame_num ^ pc_ray.random_num);

void load_g_buffer() {
    pos = gbuffer.d[pixel_idx].pos;
    normal = gbuffer.d[pixel_idx].normal;
    uv = gbuffer.d[pixel_idx].uv;
    mat_idx = gbuffer.d[pixel_idx].mat_idx;
    origin = vec4(ubo.inv_view * vec4(0, 0, 0, 1)).xyz;
}

void init_reservoir(out RestirReservoir r_new) {
    r_new.w_sum = 0;
    r_new.W = 0;
    r_new.m = 0;
}

void update_reservoir(inout RestirReservoir r_new, const RestirData s,
                      float w_i) {
    r_new.w_sum += w_i;
    r_new.m++;
    if (rand(seed) < w_i / r_new.w_sum) {
        r_new.s = s;
    }
}

vec3 calc_L(const RestirReservoir r) {
    const Material hit_mat = load_material(mat_idx, uv);
    const vec3 wo = normalize(origin - pos);
    vec2 uv_unused;
    uvec4 r_seed = r.s.seed;
    const uint light_triangle_idx = r.s.light_mesh_idx;
    const uint light_idx = r.s.light_idx;
    vec3 light_pos, light_n;
    const vec4 rands_pos =
        vec4(rand(r_seed), rand(r_seed), rand(r_seed), rand(r_seed));
    vec3 Le =
        sample_light_with_idx(rands_pos, pos, pc_ray.num_lights, light_idx,
                              light_triangle_idx, light_pos, light_n);
    vec3 wi = light_pos - pos;
    const float wi_len = length(wi);
    wi /= wi_len;
    const vec3 f = eval_bsdf(hit_mat, wo, wi, normal);
    const float cos_x = dot(normal, wi);
    const float g = abs(dot(light_n, -wi)) / (wi_len * wi_len);
    return f * Le * abs(cos_x) * g;
}

vec3 calc_L_with_visibility_check(const RestirReservoir r) {
    const Material hit_mat = load_material(mat_idx, uv);
    const vec3 wo = normalize(origin - pos);
    vec2 uv_unused;
    uvec4 r_seed = r.s.seed;
    const uint light_triangle_idx = r.s.light_mesh_idx;
    const uint light_idx = r.s.light_idx;
    vec3 light_pos, light_n;
    const vec4 rands_pos =
        vec4(rand(r_seed), rand(r_seed), rand(r_seed), rand(r_seed));
    vec3 Le = sample_light_with_idx(
        rands_pos, pos, pc_ray.num_lights, light_idx, light_triangle_idx,
        light_pos, light_n);
    vec3 wi = light_pos - pos;
    const float wi_len = length(wi);
    wi /= wi_len;
    const vec3 f = eval_bsdf(hit_mat, wo, wi, normal);
    const float cos_x = dot(normal, wi);
    const float g = abs(dot(light_n, -wi)) / (wi_len * wi_len);
    any_hit_payload.hit = 1;
    traceRayEXT(tlas,
                gl_RayFlagsTerminateOnFirstHitEXT |
                    gl_RayFlagsSkipClosestHitShaderEXT,
                0xFF, 1, 0, 1, offset_ray(pos, normal), 0, wi, wi_len - EPS, 1);
    bool visible = any_hit_payload.hit == 0;
    if (visible) {
        return f * Le * abs(cos_x) * g;
    }
    return vec3(0);
}

float calc_p_hat(const RestirReservoir r_new) { return length(calc_L(r_new)); }

void combine_reservoir(inout RestirReservoir r1, const RestirReservoir r2) {
    float fac = r2.W * r2.m;
    if (fac > 0) {
        fac *= calc_p_hat(r2);
    }
    update_reservoir(r1, r2.s, fac);
}