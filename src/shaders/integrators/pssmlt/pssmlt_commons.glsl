#ifndef PSSMLT_UTILS
#define PSSMLT_UTILS
#include "../../commons.glsl"
layout(push_constant) uniform _PushConstantRay { PCMLT pc_ray; };
layout(constant_id = 0) const int SEEDING = 0;
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer BootstrapData { BootstrapSample d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer SeedsData { SeedData d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer PrimarySamples { PrimarySample d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer MLTSamplers { MLTSampler d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer MLTColor { vec3 d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer ChainStats { ChainData d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer Splats { Splat d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer LightVertices { MLTPathVertex d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer CameraVertices { MLTPathVertex d[]; };

LightVertices light_verts = LightVertices(scene_desc.light_path_addr);
CameraVertices camera_verts = CameraVertices(scene_desc.camera_path_addr);
MLTSamplers mlt_samplers = MLTSamplers(scene_desc.mlt_samplers_addr);
MLTColor mlt_col = MLTColor(scene_desc.mlt_col_addr);
ChainStats chain_stats = ChainStats(scene_desc.chain_stats_addr);
Splats splat_data = Splats(scene_desc.splat_addr);
Splats past_splat_data = Splats(scene_desc.past_splat_addr);
BootstrapData bootstrap_data = BootstrapData(scene_desc.bootstrap_addr);
SeedsData seeds_data = SeedsData(scene_desc.seeds_addr);
PrimarySamples light_primary_samples =
    PrimarySamples(scene_desc.light_primary_samples_addr);
PrimarySamples cam_primary_samples =
    PrimarySamples(scene_desc.cam_primary_samples_addr);
PrimarySamples connection_primary_samples =
    PrimarySamples(scene_desc.connection_primary_samples_addr);
PrimarySamples prim_samples[3] = PrimarySamples[](
    light_primary_samples, cam_primary_samples, connection_primary_samples);
const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;
#define RR_MIN_DEPTH 3
uvec4 seed = init_rng(gl_LaunchIDEXT.xy, gl_LaunchSizeEXT.xy,
                      pc_ray.frame_num ^ pc_ray.random_num);
uint screen_size = gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y;
uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);
uint splat_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
                 ((pc_ray.max_depth * (pc_ray.max_depth + 1)));
uint bdpt_path_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    (pc_ray.max_depth + 1);

uint mlt_sampler_idx = pixel_idx;
uint light_primary_sample_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    pc_ray.light_rand_count;
uint cam_primary_sample_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    pc_ray.cam_rand_count;
uint connection_primary_sample_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    pc_ray.connection_rand_count;
uint prim_sample_idxs[3] =
    uint[](light_primary_sample_idx, cam_primary_sample_idx,
           connection_primary_sample_idx);

#define mlt_sampler mlt_samplers.d[mlt_sampler_idx]
#define primary_sample(i)                                                      \
    prim_samples[mlt_sampler.type].d[prim_sample_idxs[mlt_sampler.type] + i]

#define BDPT_MLT 1
bool large_step, save_radiance;
uvec4 mlt_seed;
#include "../bdpt_commons.glsl"

float mlt_L(const vec4 origin, const float cam_area) {
#define mlt_sampler mlt_samplers.d[mlt_sampler_idx]
#define splat(i) splat_data.d[splat_idx + i]
    float lum_sum = 0;
    if (save_radiance) {
        mlt_start_chain(0);
    }
    int num_light_paths = bdpt_generate_light_subpath(pc_ray.max_depth + 1);
    if (save_radiance) {
        mlt_start_chain(1);
    }
    vec2 unused;
    int num_cam_paths = bdpt_generate_camera_subpath(
        unused, origin.xyz, pc_ray.max_depth + 1, cam_area);
    if (save_radiance) {
        mlt_start_chain(2);
    }
    vec3 L = vec3(0);

    for (int t = 1; t <= num_cam_paths; t++) {
        for (int s = 0; s <= num_light_paths; s++) {
            int depth = s + t - 2;
            if (depth > (pc_ray.max_depth - 1) || depth < 0 ||
                (s == 1 && t == 1)) {
                continue;
            }
            if (t == 1) {
                ivec2 coords;
                vec3 splat_col = bdpt_connect_cam(s, coords);
                lum_sum += luminance(splat_col);
                if (save_radiance && luminance(splat_col) > 0) {
                    uint idx = coords.x * pc_ray.size_y + coords.y;
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
        const uint idx = camera_verts.d[bdpt_path_idx].coords;
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