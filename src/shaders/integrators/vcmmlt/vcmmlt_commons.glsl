#ifndef PSSMLT_UTILS
#define PSSMLT_UTILS
#include "../../bda.glsl"
#include "../../commons.glsl"
layout(location = 0) rayPayloadEXT HitPayload payload;
layout(location = 1) rayPayloadEXT AnyHitPayload any_hit_payload;
layout(push_constant) uniform _PushConstantRay { PCMLT pc; };
layout(constant_id = 0) const int SEEDING = 0;


uint chain = 0;
uint depth_factor = pc.max_depth * (pc.max_depth + 1);

SCENE_BUFFER(vcm_vertices, VCMVertex);
SCENE_BUFFER(mlt_samplers, VCMMLTSampler);
SCENE_BUFFER(mlt_col, vec3);
SCENE_BUFFER(chain_stats, ChainData);
SCENE_BUFFER(splat, Splat);
SCENE_BUFFER(past_splat, Splat);
SCENE_BUFFER(bootstrap, BootstrapSample);
SCENE_BUFFER(seeds, VCMMLTSeedData);
SCENE_BUFFER(light_primary_samples, PrimarySample);
SCENE_BUFFER(cam_primary_samples, PrimarySample);
SCENE_BUFFER(color_storage, vec3);
SCENE_BUFFER(photon, VCMPhotonHash);
SCENE_BUFFER(mlt_atomicsum, SumData);
const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;
#define RR_MIN_DEPTH 3
uvec4 seed = init_rng(gl_LaunchIDEXT.xy, gl_LaunchSizeEXT.xy,
                      pc.frame_num ^ pc.random_num);
uint screen_size = gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y;
uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);
uint splat_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
                 2 * ((pc.max_depth * (pc.max_depth + 1)));
uint vcm_light_path_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    (pc.max_depth + 1);
uint mlt_sampler_idx = pixel_idx * 2;
uint light_primary_sample_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    pc.light_rand_count * 2;
uint cam_primary_sample_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    pc.cam_rand_count;
uint prim_sample_idxs[2] =
    uint[](light_primary_sample_idx, cam_primary_sample_idx);

SCENE_BUFFER(path_cnt, uint);

#define mlt_sampler DEREF(mlt_samplers)[mlt_sampler_idx + chain]
#define primary_sample(i)                                                      \
    DEREF(light_primary_samples)[light_primary_sample_idx +                     \
                                  chain * pc.light_rand_count + i]

bool large_step, save_radiance;
uvec4 mlt_seed;
#define VCM_MLT 1
#include "../vcm_commons.glsl"

float eval_target(float lum, uint c) { return c == 0 ? float(lum > 0) : lum; }

float mlt_mis(float lum, float target, uint c) {
    const float num = target / DEREF(chain_stats)[c].normalization;
    const float denum = 1. / DEREF(chain_stats)[0].normalization +
                        lum / DEREF(chain_stats)[1].normalization;
    return num / denum;
}

float mlt_trace_eye() {
    vec3 origin = vec3(ubo.inv_view * vec4(0, 0, 0, 1));
    vec4 area_int = (ubo.inv_projection * vec4(2. / gl_LaunchSizeEXT.x,
                                               2. / gl_LaunchSizeEXT.y, 0, 1));
    area_int /= area_int.w;
    const float cam_area = abs(area_int.x * area_int.y);
    VCMState camera_state;
    // Generate camera sample
    const vec2 dir_rnd =
        vec2(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step)) *
            2.0 -
        1.0;
    const vec3 direction = sample_camera(dir_rnd).xyz;
    camera_state.wi = direction;
    camera_state.pos = origin.xyz;
    camera_state.throughput = vec3(1.0);
    camera_state.n_s = vec3(-ubo.inv_view * vec4(0, 0, 1, 0));
    float cos_theta = abs(dot(camera_state.n_s, direction));
    // Defer r^2 / cos term
    // Temporary hack?
    // TODO: Investigate
    camera_state.d_vcm =
        cam_area * screen_size * cos_theta * cos_theta * cos_theta;
    camera_state.d_vc = 0;
    camera_state.d_vm = 0;
    float lum = 0;
    vec3 col = vcm_trace_eye(camera_state, 0, 0, 0, lum);
    const float connect_lum = luminance(col);
    lum += connect_lum;
    if (save_radiance && connect_lum > 0) {
#define splat(i) DEREF(splat)[splat_idx + i]
        ivec2 coords =
            ivec2(0.5 * (1 + dir_rnd) * vec2(pc.width, pc.height));
        const uint idx = coords.x * pc.height + coords.y;
        const uint splat_cnt = mlt_sampler.splat_cnt;
        mlt_sampler.splat_cnt++;
        splat(splat_cnt).idx = idx;
        splat(splat_cnt).L = col;
#undef splat
    }
    return lum;
}

#endif
