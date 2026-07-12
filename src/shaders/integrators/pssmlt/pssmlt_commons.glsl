#ifndef PSSMLT_UTILS
#define PSSMLT_UTILS
#include "../../bda.glsl"
#include "../../commons.glsl"
layout(push_constant) uniform _PushConstantRay { PCMLT pc; };
layout(constant_id = 0) const int SEEDING = 0;
layout(location = 0) rayPayloadEXT HitPayload payload;
layout(location = 1) rayPayloadEXT AnyHitPayload any_hit_payload;

SCENE_BUFFER(light_path, MLTPathVertex);
SCENE_BUFFER(camera_path, MLTPathVertex);
SCENE_BUFFER(mlt_samplers, MLTSampler);
SCENE_BUFFER(mlt_col, vec3);
SCENE_BUFFER(chain_stats, ChainData);
SCENE_BUFFER(splat, Splat);
SCENE_BUFFER(past_splat, Splat);
SCENE_BUFFER(bootstrap, BootstrapSample);
SCENE_BUFFER(seeds, SeedData);
SCENE_BUFFER(light_primary_samples, PrimarySample);
SCENE_BUFFER(cam_primary_samples, PrimarySample);
SCENE_BUFFER(connection_primary_samples, PrimarySample);
const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;
#define RR_MIN_DEPTH 3
uvec4 seed = init_rng(gl_LaunchIDEXT.xy, gl_LaunchSizeEXT.xy,
                      pc.frame_num ^ pc.random_num);
uint screen_size = gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y;
uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);
uint splat_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
                 ((pc.max_depth * (pc.max_depth + 1)));
uint bdpt_path_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    (pc.max_depth + 1);

uint mlt_sampler_idx = pixel_idx;
uint light_primary_sample_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    pc.light_rand_count;
uint cam_primary_sample_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    pc.cam_rand_count;
uint connection_primary_sample_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    pc.connection_rand_count;
uint prim_sample_idxs[3] =
    uint[](light_primary_sample_idx, cam_primary_sample_idx,
           connection_primary_sample_idx);

#define mlt_sampler DEREF(mlt_samplers)[mlt_sampler_idx]

#define BDPT_MLT 1
bool large_step, save_radiance;
uvec4 mlt_seed;
#include "../bdpt_commons.glsl"

float mlt_L(const vec4 origin, const float cam_area) {
#define mlt_sampler DEREF(mlt_samplers)[mlt_sampler_idx]
#define splat(i) DEREF(splat)[splat_idx + i]
    float lum_sum = 0;
    if (save_radiance) {
        mlt_start_chain(0);
    }
    int num_light_paths = bdpt_generate_light_subpath(pc.max_depth + 1);
    if (save_radiance) {
        mlt_start_chain(1);
    }
    vec2 unused;
    int num_cam_paths = bdpt_generate_camera_subpath(
        unused, origin.xyz, pc.max_depth + 1, cam_area);
    if (save_radiance) {
        mlt_start_chain(2);
    }
    vec3 L = vec3(0);

    for (int t = 1; t <= num_cam_paths; t++) {
        for (int s = 0; s <= num_light_paths; s++) {
            int depth = s + t - 2;
            if (depth > (pc.max_depth - 1) || depth < 0 ||
                (s == 1 && t == 1)) {
                continue;
            }
            if (t == 1) {
                ivec2 coords;
                vec3 splat_col = bdpt_connect_cam(s, coords);
                lum_sum += luminance(splat_col);
                if (save_radiance && luminance(splat_col) > 0) {
                    uint idx = coords.x * pc.height + coords.y;
                    const uint splat_cnt = mlt_sampler.splat_cnt;
                    mlt_sampler.splat_cnt++;
                    splat(splat_cnt).idx = idx;
                    splat(splat_cnt).L = splat_col;
                }
            } else {
                L += bdpt_connect(s, t);
              
            }
        }
    }
    const float eye_lum = luminance(L);
    if (save_radiance && eye_lum > 0) {
        const uint idx = DEREF(camera_path)[bdpt_path_idx].coords;
        const uint splat_cnt = mlt_sampler.splat_cnt;
        mlt_sampler.splat_cnt++;
        splat(splat_cnt).idx = idx;
        splat(splat_cnt).L = L;
    }
#undef mlt_sampler
#undef splat
    return lum_sum + eye_lum;
}

#endif
