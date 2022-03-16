#ifndef PSSMLT_UTILS
#define PSSMLT_UTILS
#include "../../commons.glsl"
layout(push_constant) uniform _PushConstantRay { PushConstantRay pc_ray; };
layout(constant_id = 0) const int SEEDING = 0;
layout(buffer_reference, scalar) buffer BootstrapData { BootstrapSample d[]; };
layout(buffer_reference, scalar) buffer SeedsData { SeedData d[]; };
layout(buffer_reference, scalar) buffer PrimarySamples { PrimarySample d[]; };
layout(buffer_reference, scalar) buffer MLTSamplers { MLTSampler d[]; };
layout(buffer_reference, scalar) buffer MLTColor { vec3 d[]; };
layout(buffer_reference, scalar) buffer ChainStats { ChainData d[]; };
layout(buffer_reference, scalar) buffer Splats { Splat d[]; };
layout(buffer_reference, scalar) buffer LightVertices { VCMVertex d[]; };
layout(buffer_reference, scalar) buffer CameraVertices { VCMVertex d[]; };
layout(buffer_reference, scalar) buffer PathCnt { uint d[]; };
layout(buffer_reference, scalar) buffer ConnectedLights { uint d[]; };
layout(buffer_reference, scalar) buffer TmpSeeds { SeedData d[]; };
layout(buffer_reference, scalar) buffer TmpLuminance { float d[]; };
layout(buffer_reference, scalar) buffer ProbCarryover { uint d[]; };
layout(buffer_reference, scalar) buffer LightSplats { Splat d[]; };
layout(buffer_reference, scalar) buffer LightSplatCnts { uint d[]; };

TmpSeeds tmp_seeds_data = TmpSeeds(scene_desc.tmp_seeds_addr);
TmpLuminance tmp_lum_data = TmpLuminance(scene_desc.tmp_lum_addr);
ProbCarryover prob_carryover_data =
    ProbCarryover(scene_desc.prob_carryover_addr);
LightSplats light_splats = LightSplats(scene_desc.light_splats_addr);
LightSplatCnts light_splat_cnts =
    LightSplatCnts(scene_desc.light_splat_cnts_addr);

LightVertices light_verts = LightVertices(scene_desc.vcm_vertices_addr);
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

PathCnt path_cnts = PathCnt(scene_desc.path_cnt_addr);

#define mlt_sampler mlt_samplers.d[mlt_sampler_idx]
#define primary_sample(i)                                                      \
    prim_samples[mlt_sampler.type].d[prim_sample_idxs[mlt_sampler.type] + i]

uint mlt_get_next() {
    uint cnt;
    if (mlt_sampler.type == 0) {
        cnt = mlt_sampler.num_light_samples++;
    } else {
        cnt = mlt_sampler.num_cam_samples++;
    }
    return cnt;
}

uint mlt_get_sample_count() {
    uint cnt;
    if (mlt_sampler.type == 0) {
        cnt = mlt_sampler.num_light_samples;
    } else {
        cnt = mlt_sampler.num_cam_samples;
    }
    return cnt;
}

void mlt_start_iteration() { mlt_sampler.iter++; }

void mlt_select_type(uint type) { mlt_sampler.type = type; }

void mlt_start_chain(uint type) {
    mlt_sampler.type = type;
    if (mlt_sampler.type == 0) {
        mlt_sampler.num_light_samples = 0;
    } else {
        mlt_sampler.num_cam_samples = 0;
    }
}

float mlt_rand(inout uvec4 seed, bool large_step) {
    if (SEEDING == 1) {
        return rand(seed);
    }
    const uint cnt = mlt_get_next();
    const float sigma = 0.01;
    if (primary_sample(cnt).last_modified < mlt_sampler.last_large_step) {
        primary_sample(cnt).val = rand(seed);
        primary_sample(cnt).last_modified = mlt_sampler.last_large_step;
    }
    // Backup current sample
    primary_sample(cnt).backup = primary_sample(cnt).val;
    primary_sample(cnt).last_modified_backup =
        primary_sample(cnt).last_modified;
    if (large_step) {
        primary_sample(cnt).val = rand(seed);
    } else {
        uint diff = mlt_sampler.iter - primary_sample(cnt).last_modified;
        float nrm_sample = sqrt2 * erf_inv(2 * rand(seed) - 1);
        float eff_sigma = sigma * sqrt(float(diff));
        primary_sample(cnt).val += nrm_sample * eff_sigma;
        primary_sample(cnt).val -= floor(primary_sample(cnt).val);
    }
    primary_sample(cnt).last_modified = mlt_sampler.iter;
    return primary_sample(cnt).val;
}

void mlt_accept(bool large_step) {

    if (large_step) {
        mlt_sampler.last_large_step = mlt_sampler.iter;
    }
}
void mlt_reject() {
    const uint light_sample_cnt = mlt_sampler.num_light_samples;
    mlt_select_type(0);
    for (int i = 0; i < light_sample_cnt; i++) {
        // Restore
        if (primary_sample(i).last_modified == mlt_sampler.iter) {
            primary_sample(i).val = primary_sample(i).backup;
            primary_sample(i).last_modified =
                primary_sample(i).last_modified_backup;
        }
    }
    const uint cam_sample_cnt = mlt_sampler.num_cam_samples;
    mlt_select_type(1);
    for (int i = 0; i < light_sample_cnt; i++) {
        // Restore
        if (primary_sample(i).last_modified == mlt_sampler.iter) {
            primary_sample(i).val = primary_sample(i).backup;
            primary_sample(i).last_modified =
                primary_sample(i).last_modified_backup;
        }
    }
    mlt_sampler.iter--;
}

vec3 vcm_connect_cam(const vec3 cam_pos, const vec3 cam_nrm, const vec3 nrm,
                     const float cam_A, const vec3 pos, const in VCMState state,
                     const vec3 wo, const MaterialProps mat, out ivec2 coords) {
    vec3 L = vec3(0);
    vec3 dir = cam_pos - pos;
    float len = length(dir);
    dir /= len;
    float cos_y = dot(dir, nrm);
    float cos_theta = dot(cam_nrm, -dir);
    if (cos_theta <= 0.) {
        return L;
    }

    float cos_3_theta = cos_theta * cos_theta * cos_theta;
    const float cam_pdf_ratio = abs(cos_y) / (cam_A * cos_3_theta * len * len);
    vec3 ray_origin = offset_ray(pos, nrm);
    float pdf_rev, pdf_fwd;
    const vec3 f = eval_bsdf(nrm, wo, mat, 0, dot(payload.shading_nrm, wo) > 0,
                             dir, pdf_fwd, pdf_rev, cos_y);
    if (f == vec3(0)) {
        return L;
    }
    if (cam_pdf_ratio > 0.0) {
        any_hit_payload.hit = 1;
        traceRayEXT(tlas,
                    gl_RayFlagsTerminateOnFirstHitEXT |
                        gl_RayFlagsSkipClosestHitShaderEXT,
                    0xFF, 1, 0, 1, ray_origin, 0, dir, len - EPS, 1);
        if (any_hit_payload.hit == 0) {
            const float w_light = (cam_pdf_ratio / screen_size) *
                                  (state.d_vcm + pdf_rev * state.d_vc);
            const float mis_weight = 1. / (1. + w_light);
            L = mis_weight * state.throughput * cam_pdf_ratio * f / screen_size;
        }
    }
    dir = -dir;
    vec4 target = ubo.view * vec4(dir.x, dir.y, dir.z, 0);
    target /= target.z;
    target = -ubo.projection * target;
    vec2 screen_dims = vec2(pc_ray.size_x, pc_ray.size_y);
    coords = ivec2(0.5 * (1 + target.xy) * screen_dims - 0.5);
    if (coords.x < 0 || coords.x >= pc_ray.size_x || coords.y < 0 ||
        coords.y >= pc_ray.size_y || dot(dir, cam_nrm) < 0) {
        return vec3(0);
    }
    return L;
}

bool vcm_generate_light_sample(out VCMState light_state, inout uvec4 seed,
                               bool large_step) {
    // Sample light
    uint light_idx;
    uint light_triangle_idx;
    Light light;
    uint light_material_idx;
    vec2 uv_unused;
    const vec4 rands_pos =
        vec4(mlt_rand(seed, large_step), mlt_rand(seed, large_step),
             mlt_rand(seed, large_step), mlt_rand(seed, large_step));
    const TriangleRecord record =
        sample_area_light(rands_pos, pc_ray.num_lights, light_idx,
                          light_triangle_idx, light_material_idx, light);
    const MaterialProps light_mat =
        load_material(light_material_idx, uv_unused);
    vec3 wi = sample_cos_hemisphere(
        vec2(mlt_rand(seed, large_step), mlt_rand(seed, large_step)),
        record.triangle_normal);
    float pdf_pos = record.triangle_pdf * (1.0 / pc_ray.light_triangle_count);
    float cos_theta = abs(dot(wi, record.triangle_normal));
    float pdf_dir = cos_theta / PI;
    if (pdf_dir <= EPS) {
        return false;
    }
    light_state.pos = record.pos;
    light_state.shading_nrm = record.triangle_normal;
    light_state.area = 1.0 / record.triangle_pdf;
    light_state.wi = wi;
    light_state.throughput =
        light_mat.emissive_factor * cos_theta / (pdf_dir * pdf_pos);
    light_state.d_vcm = PI / cos_theta;
    light_state.d_vc = cos_theta / (pdf_dir * pdf_pos);
    return true;
}

float mlt_fill_light_path(const vec4 origin, const float cam_area,
                          bool large_step, inout uvec4 seed,
                          bool save_radiance) {
#define light_vtx(i) light_verts.d[vcm_light_path_idx + i]
#define splat(i) light_splats.d[splat_idx + i]
    VCMState light_state;
    connected_lights.d[pixel_idx] = 0;
    float lum_sum = 0;

    if (!vcm_generate_light_sample(light_state, seed, large_step)) {
        return 0;
    }
    const vec3 cam_pos = origin.xyz;
    const vec3 cam_nrm = vec3(-ubo.inv_view * vec4(0, 0, 1, 0));
    int depth;
    int path_idx = 0;
    for (depth = 1;; depth++) {
        traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, light_state.pos, tmin,
                    light_state.wi, tmax, 0);
        if (payload.material_idx == -1) {
            break;
        }
        vec3 wo = light_state.pos - payload.pos;

        vec3 shading_nrm = payload.shading_nrm;
        float cos_wo = dot(wo, shading_nrm);
        vec3 geometry_nrm = payload.geometry_nrm;
        bool side = true;
        if (dot(payload.geometry_nrm, wo) <= 0.)
            geometry_nrm = -geometry_nrm;
        if (cos_wo <= 0.) {
            cos_wo = -cos_wo;
            shading_nrm = -shading_nrm;
            side = false;
        }

        if (dot(geometry_nrm, wo) * dot(shading_nrm, wo) <= 0) {
            break;
        }
        float dist = length(payload.pos - light_state.pos);
        float dist_sqr = dist * dist;
        wo /= dist;
        const MaterialProps mat =
            load_material(payload.material_idx, payload.uv);
        const bool mat_specular =
            (mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR;
        float cos_theta_wo = abs(dot(wo, shading_nrm));
        light_state.d_vcm *= dist_sqr;
        light_state.d_vcm /= cos_theta_wo;
        light_state.d_vc /= cos_theta_wo;
        if (!mat_specular) {
            // Copy to light vertex buffer
            // light_vtx(path_idx).wi = light_state.wi;
            // light_vtx(path_idx).shading_nrm = light_state.shading_nrm;
            // light_vtx(path_idx).pos = light_state.pos;
            // light_vtx(path_idx).uv = light_state.uv;
            // light_vtx(path_idx).throughput = light_state.throughput;
            // light_vtx(path_idx).material_idx = light_state.material_idx;
            // light_vtx(path_idx).area = light_state.area;
            // light_vtx(path_idx).d_vcm = light_state.d_vcm;
            // light_vtx(path_idx).d_vc = light_state.d_vc;
            // light_vtx(path_idx).path_len = depth + 1;
            // light_vtx(path_idx).side = uint(side);
            light_vtx(path_idx).wo = wo; //-vcm_state.wi;
            light_vtx(path_idx).shading_nrm = shading_nrm;
            light_vtx(path_idx).pos = offset_ray(payload.pos, shading_nrm);
            light_vtx(path_idx).uv = payload.uv;
            light_vtx(path_idx).material_idx = payload.material_idx;
            light_vtx(path_idx).area = payload.area;
            light_vtx(path_idx).throughput = light_state.throughput;
            light_vtx(path_idx).d_vcm = light_state.d_vcm;
            light_vtx(path_idx).d_vc = light_state.d_vc;
            light_vtx(path_idx).path_len = depth + 1;
            light_vtx(path_idx).side = uint(side);
            path_idx++;
        }
        if (depth >= pc_ray.max_depth) {
            break;
        }
        if (!mat_specular && depth < pc_ray.max_depth) {
            // Connect to camera
            ivec2 coords;
            vec3 splat_col =
                vcm_connect_cam(cam_pos, cam_nrm, shading_nrm, cam_area,
                                payload.pos, light_state, wo, mat, coords);
            const float lum = luminance(splat_col);
            if (save_radiance && lum > 0) {
                connected_lights.d[pixel_idx]++;
                uint idx = coords.x * pc_ray.size_y + coords.y;
                const uint splat_cnt = light_splat_cnts.d[pixel_idx];
                light_splat_cnts.d[pixel_idx]++;
                splat(splat_cnt).idx = idx;
                splat(splat_cnt).L = splat_col;
            } else if (lum > 0) {
                connected_lights.d[pixel_idx]++;
                lum_sum += lum;
            }
        }

        // Continue the walk
        float pdf_dir;
        float cos_theta;
        vec2 rands_dir =
            vec2(mlt_rand(seed, large_step), mlt_rand(seed, large_step));
        const vec3 f =
            sample_bsdf(shading_nrm, wo, mat, 0, side, light_state.wi, pdf_dir,
                        cos_theta, rands_dir);
        const bool same_hemisphere =
            same_hemisphere(light_state.wi, wo, shading_nrm);

        const bool mat_transmissive =
            (mat.bsdf_props & BSDF_TRANSMISSIVE) == BSDF_TRANSMISSIVE;
        if (f == vec3(0) || pdf_dir == 0 ||
            (!same_hemisphere && !mat_transmissive)) {
            break;
        }

        float pdf_rev = pdf_dir;
        if (!mat_specular) {
            pdf_rev = bsdf_pdf(mat, shading_nrm, light_state.wi, wo);
        }
        const float abs_cos_theta = abs(cos_theta);

        light_state.pos = offset_ray(payload.pos, shading_nrm);
        if (!mat_specular) {
            light_state.d_vc = (abs_cos_theta / pdf_dir) *
                               (light_state.d_vcm + pdf_rev * light_state.d_vc);
            light_state.d_vcm = 1.0 / pdf_dir;
        } else {
            light_state.d_vcm = 0;
            light_state.d_vc *= abs_cos_theta;
        }

        light_state.throughput *= f * abs_cos_theta / pdf_dir;
        light_state.shading_nrm = shading_nrm;
        light_state.area = payload.area;
        light_state.material_idx = payload.material_idx;
    }
#undef light_vtx
#undef splat
    path_cnts.d[pixel_idx] = path_idx;
    return lum_sum;
}

vec3 vcm_get_light_radiance(in const MaterialProps mat,
                            in const VCMState camera_state, int d) {
    if (d == 1) {
        return mat.emissive_factor;
    }
    const float pdf_light_pos =
        1.0 / (payload.area * pc_ray.light_triangle_count);
    const float pdf_light_dir =
        abs(dot(payload.shading_nrm, -camera_state.wi)) / PI;
    const float w_camera = pdf_light_pos * camera_state.d_vcm +
                           (pdf_light_pos * pdf_light_dir) * camera_state.d_vc;

    const float mis_weight = 1. / (1. + w_camera);
    return mis_weight * mat.emissive_factor;
}

float mlt_trace_eye(const vec4 origin, const float cam_area, bool large_step,
                    inout uvec4 seed, const bool save_radiance) {
#define splat(i) splat_data.d[splat_idx + i]
    float lum = 0;
    uint light_path_idx = uint(mlt_rand(seed, large_step) * screen_size);
    uint light_splat_idx =
        light_path_idx * pc_ray.max_depth * (pc_ray.max_depth + 1);
    uint light_path_len = path_cnts.d[light_path_idx];
    mlt_sampler.splat_cnt = 0;
    if (save_radiance && connected_lights.d[light_path_idx] > 0) {
        const uint light_splat_cnt = light_splat_cnts.d[light_path_idx];
        for (int i = 0; i < light_splat_cnt; i++) {
            const vec3 light_L = light_splats.d[light_splat_idx + i].L;
            lum += luminance(light_L);
            const uint splat_cnt = mlt_sampler.splat_cnt;
            mlt_sampler.splat_cnt++;
            splat(splat_cnt).idx = light_splats.d[light_splat_idx + i].idx;
            splat(splat_cnt).L = light_L;
        }
    } else if (connected_lights.d[light_path_idx] > 0) {
        lum += tmp_lum_data.d[light_path_idx];
    }
    // if(connected_lights.d[light_path_idx] > 0) {
    //     //debugPrintfEXT("%d\n",connected_lights.d[light_path_idx]);
    //     lum /= (connected_lights.d[light_path_idx]);
    // }
    light_path_idx *= (pc_ray.max_depth + 1);
    light_splat_cnts.d[pixel_idx] = 0;
    VCMState camera_state;
    // Generate camera sample
    const vec2 dir_rnd =
        vec2(mlt_rand(seed, large_step), mlt_rand(seed, large_step)) * 2.0 -
        1.0;
    const vec3 direction = sample_camera(dir_rnd).xyz;
    camera_state.wi = direction;
    camera_state.pos = origin.xyz;
    camera_state.throughput = vec3(1.0);
    camera_state.shading_nrm = vec3(-ubo.inv_view * vec4(0, 0, 1, 0));
    float cos_theta = abs(dot(camera_state.shading_nrm, direction));
    // Defer r^2 / cos term
    // Temporary hack?
    // TODO: Investigate
    camera_state.d_vcm =
        cam_area * screen_size * cos_theta * cos_theta * cos_theta;
    camera_state.d_vc = 0;
    camera_state.d_vm = 0;
    vec3 col = vec3(0);
    int d;
    for (d = 1;; d++) {
        traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, camera_state.pos, tmin,
                    camera_state.wi, tmax, 0);
        if (payload.material_idx == -1) {
            // TODO:
            col += camera_state.throughput * pc_ray.sky_col;
            break;
        }
        vec3 wo = camera_state.pos - payload.pos;
        float dist = length(payload.pos - camera_state.pos);
        float dist_sqr = dist * dist;
        wo /= dist;
        vec3 shading_nrm = payload.shading_nrm;
        float cos_wo = dot(wo, shading_nrm);
        vec3 geometry_nrm = payload.geometry_nrm;
        bool side = true;
        if (dot(payload.geometry_nrm, wo) < 0.)
            geometry_nrm = -geometry_nrm;
        if (cos_wo < 0.) {
            cos_wo = -cos_wo;
            shading_nrm = -shading_nrm;
            side = false;
        }

        const MaterialProps mat =
            load_material(payload.material_idx, payload.uv);
        const bool mat_specular =
            (mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR;
        // Complete the missing geometry terms
        camera_state.d_vcm *= dist_sqr;
        camera_state.d_vcm /= cos_wo;
        camera_state.d_vc /= cos_wo;
        // Get the radiance
        if (luminance(mat.emissive_factor) > 0) {
            col += camera_state.throughput *
                   vcm_get_light_radiance(mat, camera_state, d);
            // break;
        }

        // Connect to light
        float pdf_rev;
        vec3 f;
        if (!mat_specular) {
            uint light_idx;
            uint light_triangle_idx;
            uint light_material_idx;
            vec2 uv_unused;
            Light light;
            const vec4 rands_pos =
                vec4(mlt_rand(seed, large_step), mlt_rand(seed, large_step),
                     mlt_rand(seed, large_step), mlt_rand(seed, large_step));
            const TriangleRecord record = sample_area_light(
                rands_pos, pc_ray.num_lights, light_idx,
                light_triangle_idx, light_material_idx, light);
            const MaterialProps light_mat =
                load_material(light_material_idx, uv_unused);
            vec3 wi = record.pos - payload.pos;
            float ray_len = length(wi);
            float ray_len_sqr = ray_len * ray_len;
            wi /= ray_len;
            const float cos_x = dot(wi, shading_nrm);
            const vec3 ray_origin = offset_ray(payload.pos, shading_nrm);
            any_hit_payload.hit = 1;
            float pdf_fwd;
            f = eval_bsdf(shading_nrm, wo, mat, 1, side, wi, pdf_fwd, pdf_rev,
                          cos_x);
            if (f != vec3(0)) {
                traceRayEXT(tlas,
                            gl_RayFlagsTerminateOnFirstHitEXT |
                                gl_RayFlagsSkipClosestHitShaderEXT,
                            0xFF, 1, 0, 1, ray_origin, 0, wi, ray_len - EPS, 1);
                const bool visible = any_hit_payload.hit == 0;
                if (visible) {
                    float g =
                        abs(dot(record.triangle_normal, -wi)) / (ray_len_sqr);
                    const float cos_y = dot(-wi, record.triangle_normal);
                    const float pdf_pos_dir = record.triangle_pdf * cos_y / PI;

                    const float pdf_light_w = record.triangle_pdf / g;
                    const float w_light = pdf_fwd / (pdf_light_w);
                    const float w_cam =
                        pdf_pos_dir * abs(cos_x) / (pdf_light_w * cos_y) *
                        (camera_state.d_vcm + camera_state.d_vc * pdf_rev);
                    const float mis_weight = 1. / (1. + w_light + w_cam);
                    if (mis_weight > 0) {
                        col += mis_weight * abs(cos_x) * f *
                               camera_state.throughput *
                               light_mat.emissive_factor /
                               (pdf_light_w / pc_ray.light_triangle_count);
                    }
                }
            }
        }

        if (!mat_specular) {
            // Connect to light vertices
#define light_vtx(i) light_verts.d[i]
            for (int i = 0; i < light_path_len; i++) {
                uint s = light_vtx(light_path_idx + i).path_len;
                uint depth = s + d - 1;
                if (depth >= pc_ray.max_depth) {
                    break;
                }

                vec3 dir = light_vtx(light_path_idx + i).pos - payload.pos;
                const float len = length(dir);
                const float len_sqr = len * len;
                dir /= len;
                const float cos_cam = dot(shading_nrm, dir);
                const float cos_light =
                    dot(light_vtx(light_path_idx + i).shading_nrm, -dir);
                const float G = cos_cam * cos_light / len_sqr;
                if (G > 0) {
                    float cam_pdf_fwd, light_pdf_fwd, light_pdf_rev;
                    const vec3 f_cam = eval_bsdf(shading_nrm, wo, mat, 1, side,
                                                 dir, cam_pdf_fwd, cos_cam);
                    const MaterialProps light_mat = load_material(
                        light_vtx(light_path_idx + i).material_idx,
                        light_vtx(light_path_idx + i).uv);
                    // TODO: what about anisotropic BSDFS?
                    const vec3 f_light = eval_bsdf(
                        light_vtx(light_path_idx + i).shading_nrm,
                        light_vtx(light_path_idx + i).wo, light_mat, 0,
                        light_vtx(light_path_idx + i).side == 1, -dir,
                        light_pdf_fwd, light_pdf_rev, cos_light);
                    if (f_light != vec3(0) && f_cam != vec3(0)) {
                        cam_pdf_fwd *= abs(cos_light) / len_sqr;
                        light_pdf_fwd *= abs(cos_cam) / len_sqr;
                        const float w_light =
                            cam_pdf_fwd *
                            (light_vtx(light_path_idx + i).d_vcm +
                             light_pdf_rev *
                                 light_vtx(light_path_idx + i).d_vc);
                        const float w_camera =
                            light_pdf_fwd *
                            (camera_state.d_vcm + pdf_rev * camera_state.d_vc);
                        const float mis_weight = 1. / (1 + w_camera + w_light);
                        const vec3 ray_origin =
                            offset_ray(payload.pos, shading_nrm);
                        any_hit_payload.hit = 1;
                        traceRayEXT(tlas,
                                    gl_RayFlagsTerminateOnFirstHitEXT |
                                        gl_RayFlagsSkipClosestHitShaderEXT,
                                    0xFF, 1, 0, 1, ray_origin, 0, dir,
                                    len - EPS, 1);
                        const bool visible = any_hit_payload.hit == 0;
                        if (visible) {
                            col += mis_weight * G * camera_state.throughput *
                                   light_vtx(light_path_idx + i).throughput *
                                   f_cam * f_light;
                        }
                    }
                }
            }
        }
        if (d >= pc_ray.max_depth) {
            break;
        }
        // Scattering
        float pdf_dir;
        float cos_theta;
        vec2 rands_dir =
            vec2(mlt_rand(seed, large_step), mlt_rand(seed, large_step));
        f = sample_bsdf(shading_nrm, wo, mat, 0, side, camera_state.wi, pdf_dir,
                        cos_theta, rands_dir);

        const bool mat_transmissive =
            (mat.bsdf_props & BSDF_TRANSMISSIVE) == BSDF_TRANSMISSIVE;
        const bool same_hemisphere =
            same_hemisphere(camera_state.wi, wo, shading_nrm);
        if (f == vec3(0) || pdf_dir == 0 ||
            (!same_hemisphere && !mat_transmissive)) {
            break;
        }
        pdf_rev = pdf_dir;
        if (!mat_specular) {
            pdf_rev = bsdf_pdf(mat, shading_nrm, camera_state.wi, wo);
        }
        const float abs_cos_theta = abs(cos_theta);

        camera_state.pos = offset_ray(payload.pos, shading_nrm);
        // Note, same cancellations also occur here from now on
        // see _vcm_generate_light_sample_
        if (!mat_specular) {
            camera_state.d_vc =
                (abs(abs_cos_theta) / pdf_dir) *
                (camera_state.d_vcm + pdf_rev * camera_state.d_vc);
            camera_state.d_vcm = 1.0 / pdf_dir;
        } else {
            camera_state.d_vcm = 0;
            camera_state.d_vc *= abs_cos_theta;
        }

        camera_state.throughput *= f * abs_cos_theta / pdf_dir;
        camera_state.shading_nrm = shading_nrm;
        camera_state.area = payload.area;
    }
    const float connect_lum = luminance(col);
    lum += connect_lum;
    if (save_radiance && connect_lum > 0) {
        ivec2 coords =
            ivec2(0.5 * (1 + dir_rnd) * vec2(pc_ray.size_x, pc_ray.size_y));
        const uint idx = coords.x * pc_ray.size_y + coords.y;
        const uint splat_cnt = mlt_sampler.splat_cnt;
        mlt_sampler.splat_cnt++;
        splat(splat_cnt).idx = idx;
        splat(splat_cnt).L = col;
    }
#undef splat
    return lum;
}

#endif