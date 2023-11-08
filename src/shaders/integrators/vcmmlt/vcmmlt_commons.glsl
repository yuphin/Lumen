#ifndef PSSMLT_UTILS
#define PSSMLT_UTILS
#include "../../commons.glsl"
layout(location = 0) rayPayloadEXT HitPayload payload;
layout(location = 1) rayPayloadEXT AnyHitPayload any_hit_payload;
layout(push_constant) uniform _PushConstantRay { PCMLT pc; };
layout(constant_id = 0) const int SEEDING = 0;
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer BootstrapData { BootstrapSample d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer SeedsData { VCMMLTSeedData d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer PrimarySamples { PrimarySample d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer MLTSamplers { VCMMLTSampler d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer MLTColor { vec3 d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer ChainStats { ChainData d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer Splats { Splat d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer LightVertices { VCMVertex d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer CameraVertices { VCMVertex d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer PathCnt { uint d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer ColorStorages { vec3 d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer PhotonData_ { VCMPhotonHash d[]; };

layout(buffer_reference, scalar, buffer_reference_align = 4) buffer MLTSumData { SumData d[]; };

uint chain = 0;
uint depth_factor = pc.max_depth * (pc.max_depth + 1);

LightVertices vcm_lights = LightVertices(scene_desc.vcm_vertices_addr);
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
PrimarySamples prim_samples[2] =
    PrimarySamples[](light_primary_samples, cam_primary_samples);
ColorStorages tmp_col = ColorStorages(scene_desc.color_storage_addr);
PhotonData_ photons = PhotonData_(scene_desc.photon_addr);
MLTSumData sum_data = MLTSumData(scene_desc.mlt_atomicsum_addr);
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

PathCnt light_path_cnts = PathCnt(scene_desc.path_cnt_addr);

#define mlt_sampler mlt_samplers.d[mlt_sampler_idx + chain]
#define primary_sample(i)                                                      \
    light_primary_samples                                                      \
        .d[light_primary_sample_idx + chain * pc.light_rand_count + i]

bool large_step, save_radiance;
uvec4 mlt_seed;
#define VCM_MLT 1
#include "../vcm_commons.glsl"

float eval_target(float lum, uint c) { return c == 0 ? float(lum > 0) : lum; }

float mlt_mis(float lum, float target, uint c) {
    const float num = target / chain_stats.d[c].normalization;
    const float denum = 1. / chain_stats.d[0].normalization +
                        lum / chain_stats.d[1].normalization;
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
#define splat(i) splat_data.d[splat_idx + i]
        ivec2 coords =
            ivec2(0.5 * (1 + dir_rnd) * vec2(pc.size_x, pc.size_y));
        const uint idx = coords.x * pc.size_y + coords.y;
        const uint splat_cnt = mlt_sampler.splat_cnt;
        mlt_sampler.splat_cnt++;
        splat(splat_cnt).idx = idx;
        splat(splat_cnt).L = col;
#undef splat
    }
    return lum;
}

#endif