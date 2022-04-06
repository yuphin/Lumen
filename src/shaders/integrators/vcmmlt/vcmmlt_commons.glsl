#ifndef PSSMLT_UTILS
#define PSSMLT_UTILS
#include "../../commons.glsl"
layout(push_constant) uniform _PushConstantRay { PushConstantRay pc_ray; };
layout(constant_id = 0) const int SEEDING = 0;
layout(buffer_reference, scalar) buffer BootstrapData { BootstrapSample d[]; };
layout(buffer_reference, scalar) buffer SeedsData { VCMMLTSeedData d[]; };
layout(buffer_reference, scalar) buffer PrimarySamples { PrimarySample d[]; };
layout(buffer_reference, scalar) buffer MLTSamplers { VCMMLTSampler d[]; };
layout(buffer_reference, scalar) buffer MLTColor { vec3 d[]; };
layout(buffer_reference, scalar) buffer ChainStats { ChainData d[]; };
layout(buffer_reference, scalar) buffer Splats { Splat d[]; };
layout(buffer_reference, scalar) buffer LightVertices { VCMVertex d[]; };
layout(buffer_reference, scalar) buffer CameraVertices { VCMVertex d[]; };
layout(buffer_reference, scalar) buffer PathCnt { uint d[]; };
layout(buffer_reference, scalar) buffer ColorStorages { vec3 d[]; };
layout(buffer_reference, scalar) buffer PhotonData_ { PhotonHash d[]; };

layout(buffer_reference, scalar) buffer MLTSumData { SumData d[]; };

uint chain = 0;
uint depth_factor = pc_ray.max_depth * (pc_ray.max_depth + 1);

LightVertices light_verts = LightVertices(scene_desc.vcm_vertices_addr);
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
                      pc_ray.frame_num ^ pc_ray.random_num);
uint screen_size = gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y;
uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);
uint splat_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
                 2 * ((pc_ray.max_depth * (pc_ray.max_depth + 1)));
uint vcm_light_path_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    (pc_ray.max_depth + 1);
uint mlt_sampler_idx = pixel_idx * 2;
uint light_primary_sample_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    pc_ray.light_rand_count * 2;
uint cam_primary_sample_idx =
    (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) *
    pc_ray.cam_rand_count;
uint prim_sample_idxs[2] =
    uint[](light_primary_sample_idx, cam_primary_sample_idx);

PathCnt path_cnts = PathCnt(scene_desc.path_cnt_addr);

vec3 normalize_grid(vec3 p, vec3 min_bnds, vec3 max_bnds) {
    return (p - min_bnds) / (max_bnds - min_bnds);
}

ivec3 get_grid_idx(vec3 p, vec3 min_bnds, vec3 max_bnds, ivec3 grid_res) {
    ivec3 res = ivec3(normalize_grid(p, min_bnds, max_bnds) * grid_res);
    clamp(res, vec3(0), grid_res - vec3(1));
    return res;
}

#define mlt_sampler mlt_samplers.d[mlt_sampler_idx + chain]
#define primary_sample(i)                                                      \
    light_primary_samples                                                      \
        .d[light_primary_sample_idx + chain * pc_ray.light_rand_count + i]

uint mlt_get_next() { return mlt_sampler.num_light_samples++; }

uint mlt_get_sample_count() { return mlt_sampler.num_light_samples; }

void mlt_start_iteration() { mlt_sampler.iter++; }

void mlt_start_chain() { mlt_sampler.num_light_samples = 0; }

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
    const uint cnt = mlt_get_sample_count();
    for (int i = 0; i < cnt; i++) {
        // Restore
        if (primary_sample(i).last_modified == mlt_sampler.iter) {
            primary_sample(i).val = primary_sample(i).backup;
            primary_sample(i).last_modified =
                primary_sample(i).last_modified_backup;
        }
    }
    mlt_sampler.iter--;
}

float eval_target(float lum, uint c) { return c == 0 ? float(lum > 0) : lum; }

float mlt_mis(float lum, float target, uint c) {
    const float num = target / chain_stats.d[c].normalization;
    const float denum = 1. / chain_stats.d[0].normalization +
                        lum / chain_stats.d[1].normalization;
    return num / denum;
}

vec3 vcm_connect_cam(const vec3 cam_pos, const vec3 cam_nrm, const vec3 nrm,
                     const float cam_A, const vec3 pos, const in VCMState state,
                     const vec3 wo, const Material mat, out ivec2 coords,
                     float eta_vm) {
    // uint size = pc_ray.size_x * pc_ray.size_y;
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
                                  (eta_vm + state.d_vcm + pdf_rev * state.d_vc);
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
    uint light_material_idx;
    vec2 uv_unused;
    Light light;
    TriangleRecord record;
    Material light_mat;
    float pdf_pos, pdf_dir;
    vec3 wi;
    const vec4 rands_pos =
        vec4(mlt_rand(seed, large_step), mlt_rand(seed, large_step),
             mlt_rand(seed, large_step), mlt_rand(seed, large_step));
    const vec2 rands_dir =
        vec2(mlt_rand(seed, large_step), mlt_rand(seed, large_step));
    const vec3 Le = sample_light_Le(
        rands_pos, rands_dir, pc_ray.num_lights, pc_ray.light_triangle_count,
        light_idx, light_triangle_idx, light_material_idx, light, record,
        light_mat, pdf_pos, seed, wi, pdf_dir);
    float cos_theta = abs(dot(wi, record.triangle_normal));
    if (pdf_dir <= EPS) {
        return false;
    }
    light_state.pos = record.pos;
    light_state.shading_nrm = record.triangle_normal;
    light_state.area = 1.0 / record.triangle_pdf;
    light_state.wi = wi;
    light_state.throughput = Le * cos_theta / (pdf_dir * pdf_pos);
    light_state.d_vcm = 1. / pdf_dir;
    light_state.d_vc = cos_theta / (pdf_dir * pdf_pos);
    return true;
}

bool vcm_generate_light_sample(float eta_vc, out VCMState light_state) {
    // Sample light
    uint light_idx;
    uint light_triangle_idx;
    uint light_material_idx;
    vec2 uv_unused;
    Light light;
    TriangleRecord record;
    Material light_mat;
    float u, v;
    vec3 wi;
    const vec3 Le =
        sample_light_Le(pc_ray.num_lights, pc_ray.light_triangle_count,
                        light_idx, light_triangle_idx, light_material_idx,
                        light, record, light_mat, seed, wi, u, v);
    float pdf_pos = record.triangle_pdf * (1.0 / pc_ray.light_triangle_count);
    float pdf_dir = light_pdf(light.light_flags, record.triangle_normal, wi);
    float cos_theta = abs(dot(wi, record.triangle_normal));
    if (pdf_dir <= EPS) {
        return false;
    }
    light_state.pos = record.pos;
    light_state.shading_nrm = record.triangle_normal;
    light_state.area = 1.0 / record.triangle_pdf;
    light_state.wi = wi;
    light_state.throughput = Le * cos_theta / (pdf_dir * pdf_pos);
    light_state.d_vcm = 1. / pdf_dir;
    light_state.d_vc = cos_theta / (pdf_dir * pdf_pos);
    light_state.d_vm = light_state.d_vc * eta_vc;
    return true;
}

vec3 vcm_get_light_radiance(in const Material mat,
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

float mlt_fill_eye_path(const vec4 origin, const float cam_area) {
#define cam_vtx(i) light_verts.d[vcm_light_path_idx + i]
    const float fov = ubo.projection[1][1];
    const vec3 cam_pos = origin.xyz;
    vec2 dir = vec2(rand(seed), rand(seed)) * 2.0 - 1.0;
    const vec3 direction = sample_camera(dir).xyz;
    VCMState camera_state;
    float lum_sum = 0;
    // Generate camera sample
    camera_state.wi = direction;
    camera_state.pos = origin.xyz;
    camera_state.throughput = vec3(1.0);
    camera_state.shading_nrm = vec3(-ubo.inv_view * vec4(0, 0, 1, 0));
    float cos_theta = abs(dot(camera_state.shading_nrm, direction));
    // Defer r^2 / cos term
    camera_state.d_vcm = cam_area * pc_ray.size_x * pc_ray.size_y * cos_theta *
                         cos_theta * cos_theta;
    camera_state.d_vc = 0;
    camera_state.d_vm = 0;
    int d;
    int path_idx = 0;
    ivec2 coords = ivec2(0.5 * (1 + dir) * vec2(pc_ray.size_x, pc_ray.size_y));
    uint coords_idx = coords.x * pc_ray.size_y + coords.y;
    for (d = 1;; d++) {
        traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, camera_state.pos, tmin,
                    camera_state.wi, tmax, 0);

        if (payload.material_idx == -1) {
            // TODO:
            tmp_col.d[coords_idx] += camera_state.throughput * pc_ray.sky_col;
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

        const Material mat =
            load_material(payload.material_idx, payload.uv);
        const bool mat_specular =
            (mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR;
        // Complete the missing geometry terms
        camera_state.d_vcm *= dist_sqr;
        camera_state.d_vcm /= cos_wo;
        camera_state.d_vc /= cos_wo;
        camera_state.d_vm /= cos_wo;

        // Get the radiance
        if (luminance(mat.emissive_factor) > 0) {
            vec3 L = camera_state.throughput *
                     vcm_get_light_radiance(mat, camera_state, d);
            tmp_col.d[coords_idx] += L;
            lum_sum += luminance(L);
            // if (pc_ray.use_vc == 1 || pc_ray.use_vm == 1) {
            //     // break;
            // }
        }

        // Copy to camera vertex buffer
        if (!mat_specular) {
            cam_vtx(path_idx).wo = wo;
            cam_vtx(path_idx).shading_nrm = shading_nrm;
            cam_vtx(path_idx).pos = offset_ray(payload.pos, shading_nrm);
            cam_vtx(path_idx).uv = payload.uv;
            cam_vtx(path_idx).material_idx = payload.material_idx;
            cam_vtx(path_idx).area = payload.area;
            cam_vtx(path_idx).throughput = camera_state.throughput;
            cam_vtx(path_idx).d_vcm = camera_state.d_vcm;
            cam_vtx(path_idx).d_vc = camera_state.d_vc;
            cam_vtx(path_idx).d_vm = camera_state.d_vm;
            cam_vtx(path_idx).path_len = d + 1;
            cam_vtx(path_idx).side = uint(side);
            cam_vtx(path_idx).coords = coords_idx;
            path_idx++;
        }

        // Connect to light
        float pdf_rev;
        vec3 f;
        if (!mat_specular) {
            uint light_idx;
            uint light_triangle_idx;
            uint light_material_idx;
            Light light;
            TriangleRecord record;
            Material light_mat;
            const vec3 Le = sample_light(
                payload.pos, pc_ray.num_lights, light_idx, light_triangle_idx,
                light_material_idx, light, record, light_mat, seed);
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
                    const float pdf_dir =
                        light_pdf(light, record.triangle_normal, -wi);
                    float g =
                        abs(dot(record.triangle_normal, -wi)) / (ray_len_sqr);
                    const float cos_y = dot(-wi, record.triangle_normal);
                    const float pdf_pos_dir = record.triangle_pdf * pdf_dir;

                    const float pdf_light_w = record.triangle_pdf / g;
                    const float w_light = pdf_fwd / (pdf_light_w);
                    const float w_cam =
                        pdf_pos_dir * abs(cos_x) / (pdf_light_w * cos_y) *
                        (camera_state.d_vcm + camera_state.d_vc * pdf_rev);
                    const float mis_weight = 1. / (1. + w_light + w_cam);
                    if (mis_weight > 0) {
                        vec3 L = mis_weight * abs(cos_x) * f *
                                 camera_state.throughput * Le /
                                 (pdf_light_w / pc_ray.light_triangle_count);
                        tmp_col.d[coords_idx] += L;
                        lum_sum += luminance(L);
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
        f = sample_bsdf(shading_nrm, wo, mat, 0, side, camera_state.wi, pdf_dir,
                        cos_theta, seed);

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
        camera_state.material_idx = payload.material_idx;
    }
    path_cnts.d[pixel_idx] = path_idx;
#undef cam_vtx
    return lum_sum;
}

float mlt_fill_light_path(const vec4 origin, const float cam_area) {
#define light_vtx(i) light_verts.d[vcm_light_path_idx + i]
    VCMState light_state;
    const float radius = pc_ray.radius;
    const float radius_sqr = radius * radius;
    float eta_vcm = PI * radius_sqr * pc_ray.size_x * pc_ray.size_y;
    float eta_vc = 1.0 / eta_vcm;
    float eta_vm = (pc_ray.use_vm == 1)
                       ? PI * radius_sqr * pc_ray.size_x * pc_ray.size_y
                       : 0;
    float lum_sum = 0;
    if (!vcm_generate_light_sample(eta_vc, light_state)) {
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
        const Material mat =
            load_material(payload.material_idx, payload.uv);
        const bool mat_specular =
            (mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR;
        float cos_theta_wo = abs(dot(wo, shading_nrm));
        light_state.d_vcm *= dist_sqr;
        light_state.d_vcm /= cos_theta_wo;
        light_state.d_vc /= cos_theta_wo;
        light_state.d_vm /= cos_theta_wo;
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
            // light_vtx(path_idx).d_vm = light_state.d_vm;
            // light_vtx(path_idx).path_len = depth;
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
            vec3 splat_col = vcm_connect_cam(cam_pos, cam_nrm, shading_nrm,
                                             cam_area, payload.pos, light_state,
                                             wo, mat, coords, eta_vm);
            const float lum = luminance(splat_col);
            if (lum > 0) {
                lum_sum += lum;
                uint idx = coords.x * pc_ray.size_y + coords.y;
                tmp_col.d[idx] += splat_col;
            }
        }

        // Continue the walk
        float pdf_dir;
        float cos_theta;
        vec2 rands_dir = vec2(rand(seed), rand(seed));
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
            light_state.d_vc =
                (abs_cos_theta / pdf_dir) *
                (eta_vm + light_state.d_vcm + pdf_rev * light_state.d_vc);
            light_state.d_vm =
                (abs_cos_theta / pdf_dir) *
                (1 + light_state.d_vcm * eta_vc + pdf_rev * light_state.d_vm);
            light_state.d_vcm = 1.0 / pdf_dir;
        } else {
            light_state.d_vcm = 0;
            light_state.d_vc *= abs_cos_theta;
            light_state.d_vm *= abs_cos_theta;
        }

        light_state.throughput *= f * abs_cos_theta / pdf_dir;
        light_state.shading_nrm = shading_nrm;
        light_state.area = payload.area;
        light_state.material_idx = payload.material_idx;
    }
    path_cnts.d[pixel_idx] = path_idx;
    if (pc_ray.use_vm == 1) {
        for (int i = 0; i < path_idx; i++) {
            ivec3 grid_idx = get_grid_idx(light_vtx(i).pos, pc_ray.min_bounds,
                                          pc_ray.max_bounds, pc_ray.grid_res);
            uint h = hash(grid_idx, screen_size);
            photons.d[h].pos = light_vtx(i).pos;
            photons.d[h].wi = -light_vtx(i).wi;
            photons.d[h].d_vm = light_vtx(i).d_vm;
            photons.d[h].d_vcm = light_vtx(i).d_vcm;
            photons.d[h].throughput = light_vtx(i).throughput;
            photons.d[h].nrm = light_vtx(i).shading_nrm;
            photons.d[h].path_len = light_vtx(i).path_len;
            atomicAdd(photons.d[h].photon_count, 1);
        }
    }
#undef light_vtx
    return lum_sum;
}

float mlt_trace_eye(const vec4 origin, const float cam_area, bool large_step,
                    inout uvec4 seed, const bool save_radiance) {
#define splat(i) splat_data.d[splat_idx + chain * depth_factor + i]
    const uint num_light_paths = pc_ray.size_x * pc_ray.size_y;
    const float radius = pc_ray.radius;
    const float radius_sqr = radius * radius;
    float eta_vcm = PI * radius_sqr * num_light_paths;
    float eta_vc = 1.0 / eta_vcm;
    float eta_vm = pc_ray.use_vm == 1 ? PI * radius_sqr * num_light_paths : 0;
    const float normalization_factor = 1. / (PI * radius_sqr * num_light_paths);
    float lum = 0;
    uint light_path_idx = uint(mlt_rand(seed, large_step) * num_light_paths);
    uint light_splat_idx =
        light_path_idx * pc_ray.max_depth * (pc_ray.max_depth + 1);
    uint light_path_len = path_cnts.d[light_path_idx];
    mlt_sampler.splat_cnt = 0;
    light_path_idx *= (pc_ray.max_depth + 1);
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
        cam_area * num_light_paths * cos_theta * cos_theta * cos_theta;
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

        const Material mat =
            load_material(payload.material_idx, payload.uv);
        const bool mat_specular =
            (mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR;
        // Complete the missing geometry terms
        camera_state.d_vcm *= dist_sqr;
        camera_state.d_vcm /= cos_wo;
        camera_state.d_vc /= cos_wo;
        camera_state.d_vm /= cos_wo;
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
            Light light;
            TriangleRecord record;
            Material light_mat;
            const vec4 rands_pos =
                vec4(mlt_rand(seed, large_step), mlt_rand(seed, large_step),
                     mlt_rand(seed, large_step), mlt_rand(seed, large_step));
            const vec3 Le =
                sample_light(rands_pos, payload.pos, pc_ray.num_lights,
                             light_idx, light_triangle_idx, light_material_idx,
                             light, record, light_mat, seed);
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
                    const float pdf_dir =
                        light_pdf(light, record.triangle_normal, -wi);
                    float g =
                        abs(dot(record.triangle_normal, -wi)) / (ray_len_sqr);
                    const float cos_y = dot(-wi, record.triangle_normal);
                    const float pdf_pos_dir = record.triangle_pdf * pdf_dir;

                    const float pdf_light_w = record.triangle_pdf / g;
                    const float w_light = pdf_fwd / (pdf_light_w);
                    const float w_cam =
                        pdf_pos_dir * abs(cos_x) / (pdf_light_w * cos_y) *
                        (camera_state.d_vcm + camera_state.d_vc * pdf_rev);
                    const float mis_weight = 1. / (1. + w_light + w_cam);
                    if (mis_weight > 0) {
                        col += mis_weight * abs(cos_x) * f *
                               camera_state.throughput * Le /
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
                if (s == 1) {
                    continue;
                }
                vec3 dir = light_vtx(light_path_idx + i).pos - payload.pos;
                const float len = length(dir);
                const float len_sqr = len * len;
                dir /= len;
                const float cos_cam = dot(shading_nrm, dir);
                const float cos_light =
                    dot(light_vtx(light_path_idx + i).shading_nrm, -dir);
                const float G = cos_light * cos_cam / len_sqr;
                if (G > 0) {
                    float cam_pdf_fwd, light_pdf_fwd, light_pdf_rev;
                    const vec3 f_cam = eval_bsdf(shading_nrm, wo, mat, 1, side,
                                                 dir, cam_pdf_fwd, cos_cam);
                    const Material light_mat = load_material(
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
                            (eta_vm + light_vtx(light_path_idx + i).d_vcm +
                             light_pdf_rev *
                                 light_vtx(light_path_idx + i).d_vc);
                        const float w_camera =
                            light_pdf_fwd * (eta_vm + camera_state.d_vcm +
                                             pdf_rev * camera_state.d_vc);
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

        // Vertex merging
        vec3 r = vec3(radius);
        float r_sqr = radius * radius;
        if (!mat_specular && pc_ray.use_vm == 1) {
            ivec3 grid_min_bnds_idx =
                get_grid_idx(payload.pos - vec3(radius), pc_ray.min_bounds,
                             pc_ray.max_bounds, pc_ray.grid_res);
            ivec3 grid_max_bnds_idx =
                get_grid_idx(payload.pos + vec3(radius), pc_ray.min_bounds,
                             pc_ray.max_bounds, pc_ray.grid_res);
            for (int x = grid_min_bnds_idx.x; x <= grid_max_bnds_idx.x; x++) {
                for (int y = grid_min_bnds_idx.y; y <= grid_max_bnds_idx.y;
                     y++) {
                    for (int z = grid_min_bnds_idx.z; z <= grid_max_bnds_idx.z;
                         z++) {
                        const uint h = hash(ivec3(x, y, z), screen_size);
                        if (photons.d[h].photon_count > 0) {
                            const vec3 pp = payload.pos - photons.d[h].pos;
                            const float dist_sqr = dot(pp, pp);
                            if (dist_sqr > r_sqr) {
                                continue;
                            }
                            // Should we?
                            uint depth = photons.d[h].path_len + d - 1;
                            if (depth > pc_ray.max_depth) {
                                continue;
                            }
                            float cam_pdf_fwd, cam_pdf_rev;
                            const float cos_theta =
                                dot(photons.d[h].wi, shading_nrm);
                            f = eval_bsdf(shading_nrm, wo, mat, 1, side,
                                          photons.d[h].wi, cam_pdf_fwd,
                                          cam_pdf_rev, cos_theta);

                            if (f != vec3(0)) {
                                const float w_light =
                                    photons.d[h].d_vcm * eta_vc +
                                    photons.d[h].d_vm * cam_pdf_fwd;
                                const float w_cam =
                                    camera_state.d_vcm * eta_vc +
                                    camera_state.d_vm * cam_pdf_rev;
                                const float mis_weight =
                                    1. / (1 + w_light + w_cam);
                                float cos_nrm =
                                    dot(photons.d[h].nrm, shading_nrm);
                                if (cos_nrm > EPS) {
                                    const float w =
                                        1. - sqrt(dist_sqr) / radius;
                                    const float w_normalization =
                                        3.; // 1. / (1 - 2/(3*k)) where k =
                                            // 1
                                    col += w * mis_weight *
                                           photons.d[h].photon_count *
                                           photons.d[h].throughput * f *
                                           camera_state.throughput *
                                           normalization_factor *
                                           w_normalization;
                                }
                            }
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
                (eta_vm + camera_state.d_vcm + pdf_rev * camera_state.d_vc);
            camera_state.d_vm =
                (abs(abs_cos_theta) / pdf_dir) *
                (1 + camera_state.d_vcm * eta_vc + pdf_rev * camera_state.d_vm);
            camera_state.d_vcm = 1.0 / pdf_dir;
        } else {
            camera_state.d_vcm = 0;
            camera_state.d_vc *= abs_cos_theta;
            camera_state.d_vm *= abs_cos_theta;
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

float mlt_trace_light(const vec3 cam_pos, const vec3 cam_nrm,
                      const float cam_area, bool large_step, inout uvec4 seed,
                      const bool save_radiance) {
#define splat(i) splat_data.d[splat_idx + chain * depth_factor + i]
    // Select camera path
    float luminance_sum = 0;
    mlt_sampler.splat_cnt = 0;
    uint path_idx =
        uint(mlt_rand(seed, large_step) * (pc_ray.size_x * pc_ray.size_y));
    uint path_len = path_cnts.d[path_idx];
    path_idx *= (pc_ray.max_depth + 1);
    // Trace from light
    VCMState light_state;
    if (!vcm_generate_light_sample(light_state, seed, large_step)) {
        return 0;
    }
    int d;
    for (d = 1;; d++) {
        traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, light_state.pos, tmin,
                    light_state.wi, tmax, 0);
        if (payload.material_idx == -1) {
            break;
        }
        const vec3 hit_pos = payload.pos;
        vec3 wo = light_state.pos - hit_pos;

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
            // We dont handle BTDF at the moment
            break;
        }
        float dist = length(payload.pos - light_state.pos);
        float dist_sqr = dist * dist;
        wo /= dist;
        const Material mat =
            load_material(payload.material_idx, payload.uv);
        const bool mat_specular =
            (mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR;
        // Complete the missing geometry terms
        float cos_theta_wo = abs(dot(wo, shading_nrm));
        // Can't connect from specular to camera path, can't merge either
        light_state.d_vcm *= dist_sqr;
        light_state.d_vcm /= cos_theta_wo;
        light_state.d_vc /= cos_theta_wo;
        light_state.d_vm /= cos_theta_wo;
        if (d >= pc_ray.max_depth + 1) {
            break;
        }
        if (d < pc_ray.max_depth) {
            // Connect to camera
            ivec2 coords;
            vec3 splat_col =
                vcm_connect_cam(cam_pos, cam_nrm, shading_nrm, cam_area,
                                payload.pos, light_state, wo, mat, coords, 0);
            const float lum_val = luminance(splat_col);
            if (lum_val > 0) {
                luminance_sum += lum_val;
                if (save_radiance) {
                    const uint idx = coords.x * pc_ray.size_y + coords.y;
                    const uint splat_cnt = mlt_sampler.splat_cnt;
                    mlt_sampler.splat_cnt++;
                    splat(splat_cnt).idx = idx;
                    splat(splat_cnt).L = splat_col;
                }
            }
        }
        vec3 unused;

        if (!mat_specular) {
#define cam_vtx(i) light_verts.d[i]
            // Connect to cam vertices
            for (int i = 0; i < path_len; i++) {
                uint t = cam_vtx(path_idx + i).path_len;
                uint depth = t + d - 1;
                if (depth >= pc_ray.max_depth) {
                    break;
                }
                vec3 dir = hit_pos - cam_vtx(path_idx + i).pos;
                const float len = length(dir);
                const float len_sqr = len * len;
                dir /= len;
                const float cos_light = dot(shading_nrm, -dir);
                const float cos_cam =
                    dot(cam_vtx(path_idx + i).shading_nrm, dir);
                const float G = cos_cam * cos_light / len_sqr;
                if (G > 0) {
                    float pdf_rev = bsdf_pdf(mat, shading_nrm, -dir, wo);
                    vec3 unused;
                    float cam_pdf_fwd, cam_pdf_rev, light_pdf_fwd;
                    const Material cam_mat =
                        load_material(cam_vtx(path_idx + i).material_idx,
                                      cam_vtx(path_idx + i).uv);
                    const vec3 f_cam =
                        eval_bsdf(cam_vtx(path_idx + i).shading_nrm,
                                  cam_vtx(path_idx + i).wo, cam_mat, 1,
                                  cam_vtx(path_idx + i).side == 1, dir,
                                  cam_pdf_fwd, cam_pdf_rev, cos_cam);
                    const vec3 f_light =
                        eval_bsdf(shading_nrm, wo, mat, 0, side, -dir,
                                  light_pdf_fwd, cos_light);
                    if (f_light != vec3(0) && f_cam != vec3(0)) {
                        cam_pdf_fwd *= abs(cos_light) / len_sqr;
                        light_pdf_fwd *= abs(cos_cam) / len_sqr;
                        const float w_light =
                            cam_pdf_fwd *
                            (light_state.d_vcm + pdf_rev * light_state.d_vc);
                        const float w_cam =
                            light_pdf_fwd *
                            (cam_vtx(path_idx + i).d_vcm +
                             cam_pdf_rev * cam_vtx(path_idx + i).d_vc);
                        const float mis_weight = 1. / (1 + w_light + w_cam);
                        const vec3 ray_origin =
                            offset_ray(hit_pos, shading_nrm);
                        any_hit_payload.hit = 1;
                        traceRayEXT(tlas,
                                    gl_RayFlagsTerminateOnFirstHitEXT |
                                        gl_RayFlagsSkipClosestHitShaderEXT,
                                    0xFF, 1, 0, 1, ray_origin, 0, -dir,
                                    len - EPS, 1);
                        const bool visible = any_hit_payload.hit == 0;
                        if (visible) {
                            const vec3 L = mis_weight * G *
                                           light_state.throughput *
                                           cam_vtx(path_idx + i).throughput *
                                           f_cam * f_light;
                            luminance_sum += luminance(L);
                            if (save_radiance) {
                                const uint idx = cam_vtx(path_idx + i).coords;
                                const uint splat_cnt = mlt_sampler.splat_cnt;
                                mlt_sampler.splat_cnt++;
                                splat(splat_cnt).idx = idx;
                                splat(splat_cnt).L = L;
                            }
                        }
                    }
                }
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
        // Note, same cancellations also occur here from now on
        // see _vcm_generate_light_sample_
        if (!mat_specular) {
            light_state.d_vc = (abs_cos_theta / pdf_dir) *
                               (light_state.d_vcm + pdf_rev * light_state.d_vc);
            light_state.d_vcm = 1.0 / pdf_dir;
        } else {
            // Specular pdf has value = inf, so d_vcm = 0;
            light_state.d_vcm = 0;
            // pdf_fwd = pdf_rev = delta -> cancels
            light_state.d_vc *= abs_cos_theta;
        }
        light_state.throughput *= f * abs_cos_theta / pdf_dir;
        light_state.shading_nrm = shading_nrm;
        light_state.area = payload.area;
        light_state.material_idx = payload.material_idx;
    }
    return luminance_sum;
#undef splat
#undef cam_vtx
}

#endif