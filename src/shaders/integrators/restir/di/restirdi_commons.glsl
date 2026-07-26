#include "../../../bda.glsl"
#include "../../../commons.glsl"
layout(location = 0) rayPayloadEXT HitPayload payload;
layout(location = 1) rayPayloadEXT AnyHitPayload any_hit_payload;
#include "../../../shadow_ray.glsl"
layout(push_constant) uniform _PushConstantRay { PCReSTIR pc; };
const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;
#define RR_MIN_DEPTH 3

SCENE_BUFFER(color_storage, vec3);
SCENE_BUFFER(g_buffer, RestirGBufferData);
SCENE_BUFFER(passthrough_reservoir, RestirReservoir);
SCENE_BUFFER(temporal_reservoir, RestirReservoir);
SCENE_BUFFER(spatial_reservoir, RestirReservoir);

vec3 pos;
vec3 normal;
vec2 uv;
uint mat_idx;
vec3 origin;

uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);
uvec4 seed = init_rng(gl_LaunchIDEXT.xy, gl_LaunchSizeEXT.xy,
                      pc.frame_num ^ pc.random_num);

void load_g_buffer() {
    pos = DEREF(g_buffer)[pixel_idx].pos;
    normal = DEREF(g_buffer)[pixel_idx].normal;
    uv = DEREF(g_buffer)[pixel_idx].uv;
    mat_idx = DEREF(g_buffer)[pixel_idx].mat_idx;
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
    const LightLiSample light_sample = replay_light_Li(r.s.identity, pos, pc.num_lights);
    // Whether it's forward facing shouldn't matter here
    const vec3 f = eval_bsdf(hit_mat, wo, light_sample.wi, normal, 1, true);
    return f * light_sample.Li * abs(dot(normal, light_sample.wi));
}

vec3 calc_L_with_visibility_check(const RestirReservoir r) {
    const Material hit_mat = load_material(mat_idx, uv);
    const vec3 wo = normalize(origin - pos);
    const LightLiSample light_sample = replay_light_Li(r.s.identity, pos, pc.num_lights);
    const vec3 f = eval_bsdf(hit_mat, wo, light_sample.wi, normal, 1, true);
    bool visible = !connection_occluded(offset_ray(pos, normal),
                                        light_sample.wi,
                                        light_sample.distance, 0xFF);
    if (visible) {
        return f * light_sample.Li * abs(dot(normal, light_sample.wi));
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
