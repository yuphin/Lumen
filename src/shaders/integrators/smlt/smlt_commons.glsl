#ifndef PSSMLT_UTILS
#define PSSMLT_UTILS
#include "../../commons.glsl"
layout(location = 0) rayPayloadEXT HitPayload payload;
layout(location = 1) rayPayloadEXT AnyHitPayload any_hit_payload;
layout(push_constant) uniform _PushConstantRay { PCMLT pc_ray; };
layout(constant_id = 0) const int SEEDING = 0;
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer BootstrapData { BootstrapSample d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer SeedsData { SeedData d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer PrimarySamples { PrimarySample d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer MLTSamplers { MLTSampler d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer MLTColor { vec3 d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer ChainStats { ChainData d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer Splats { Splat d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer LightVertices { VCMVertex d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer CameraVertices { VCMVertex d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer PathCnt { uint d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer ConnectedLights { uint d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer TmpSeeds { SeedData d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer TmpLuminance { float d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer ProbCarryover { uint d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer LightSplats { Splat d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer LightSplatCnts { uint d[]; };

TmpSeeds tmp_seeds_data = TmpSeeds(scene_desc.tmp_seeds_addr);
TmpLuminance tmp_lum_data = TmpLuminance(scene_desc.tmp_lum_addr);
ProbCarryover prob_carryover_data =
    ProbCarryover(scene_desc.prob_carryover_addr);
LightSplats light_splats = LightSplats(scene_desc.light_splats_addr);
LightSplatCnts light_splat_cnts =
    LightSplatCnts(scene_desc.light_splat_cnts_addr);

LightVertices vcm_lights = LightVertices(scene_desc.vcm_vertices_addr);
// CameraVertices camera_verts = CameraVertices(scene_desc.vcm_vertices_addr);
ConnectedLights connected_lights =
    ConnectedLights(scene_desc.connected_lights_addr);
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
uint vcm_light_path_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    (pc_ray.max_depth + 1);
uint mlt_sampler_idx = pixel_idx;
uint light_primary_sample_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    pc_ray.light_rand_count;
uint cam_primary_sample_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    pc_ray.cam_rand_count;
uint prim_sample_idxs[2] =
    uint[](light_primary_sample_idx, cam_primary_sample_idx);

PathCnt light_path_cnts = PathCnt(scene_desc.path_cnt_addr);

#define mlt_sampler mlt_samplers.d[mlt_sampler_idx]
#define primary_sample(i)                                                      \
    prim_samples[mlt_sampler.type].d[prim_sample_idxs[mlt_sampler.type] + i]

bool large_step, save_radiance;
uvec4 mlt_seed;
#define VC_MLT 1
#include "../vcm_commons.glsl"

float mlt_L_light() {
    vec3 origin = vec3(ubo.inv_view * vec4(0, 0, 0, 1));
    VCMState light_state;
    connected_lights.d[pixel_idx] = 0;
    float lum_sum = 0;
    float eta_vc = 0, eta_vm = 0;
    bool finite;
    if (!vcm_generate_light_sample(eta_vc, light_state, finite)) {
        return 0;
    }
    float res = vcm_fill_light(origin, light_state, finite, 0, eta_vc, eta_vm);
    return res;
}

float mlt_L_eye() {
    vec3 origin = vec3(ubo.inv_view * vec4(0, 0, 0, 1));
    vec4 area_int = (ubo.inv_projection * vec4(2. / gl_LaunchSizeEXT.x,
                                               2. / gl_LaunchSizeEXT.y, 0, 1));
    area_int /= area_int.w;
    const float cam_area = abs(area_int.x * area_int.y);
    VCMState camera_state;
    // Generate camera sample
    const vec2 dir_rnd =
        vec2(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step)) * 2.0 -
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
            ivec2(0.5 * (1 + dir_rnd) * vec2(pc_ray.size_x, pc_ray.size_y));
        const uint idx = coords.x * pc_ray.size_y + coords.y;
        const uint splat_cnt = mlt_sampler.splat_cnt;
        mlt_sampler.splat_cnt++;
        splat(splat_cnt).idx = idx;
        splat(splat_cnt).L = col;
#undef splat
    }
    return lum;
}
#endif