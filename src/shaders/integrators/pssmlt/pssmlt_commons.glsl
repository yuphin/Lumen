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
layout(buffer_reference, scalar) buffer LightVertices { MLTPathVertex d[]; };
layout(buffer_reference, scalar) buffer CameraVertices { MLTPathVertex d[]; };

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

uint mlt_get_next() {
    uint cnt;
    if (mlt_sampler.type == 0) {
        cnt = mlt_sampler.num_light_samples++;
    } else if (mlt_sampler.type == 1) {
        cnt = mlt_sampler.num_cam_samples++;
    } else {
        cnt = mlt_sampler.num_connection_samples++;
    }
    return cnt;
}

uint mlt_get_sample_count() {
    uint cnt;
    if (mlt_sampler.type == 0) {
        cnt = mlt_sampler.num_light_samples;
    } else if (mlt_sampler.type == 1) {
        cnt = mlt_sampler.num_cam_samples;
    } else {
        cnt = mlt_sampler.num_connection_samples;
    }
    return cnt;
}

void mlt_start_iteration() { mlt_sampler.iter++; }

void mlt_select_type(uint type) { mlt_sampler.type = type; }

void mlt_start_chain(uint type) {
    mlt_sampler.type = type;
    if (mlt_sampler.type == 0) {
        mlt_sampler.num_light_samples = 0;
    } else if (mlt_sampler.type == 1) {
        mlt_sampler.num_cam_samples = 0;
    } else {
        mlt_sampler.num_connection_samples = 0;
    }
}

float mutate(float val, inout uvec4 seed) {
    float rnd = rand(seed);
    bool add;
    const float mut_size_high = 1. / 64;
    const float mut_size_low = 1./ 1024.;
    const float log_ratio = log(mut_size_high / mut_size_low);
    if(rnd < 0.5) {
        add = true;
        rnd *= 2.0;
    } else {
        add = false;
        rnd = 2.0 * (rnd - 0.5);
    }
    float dv = mut_size_high * exp(rnd * log_ratio);
    if(add) {
        val += dv;
        if(val > 1) val -= 1;
    } else {
        val -= dv;
        if(val < 0) val += 1;
    }
    return val;
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
        float before = primary_sample(cnt).val;
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
    for (int i = 0; i < cam_sample_cnt; i++) {
        // Restore
        if (primary_sample(i).last_modified == mlt_sampler.iter) {
            primary_sample(i).val = primary_sample(i).backup;
            primary_sample(i).last_modified =
                primary_sample(i).last_modified_backup;
        }
    }
    const uint connections_cnt = mlt_sampler.num_connection_samples;
     mlt_select_type(2);
    for (int i = 0; i < connections_cnt; i++) {
        // Restore
        if (primary_sample(i).last_modified == mlt_sampler.iter) {
            primary_sample(i).val = primary_sample(i).backup;
            primary_sample(i).last_modified =
                primary_sample(i).last_modified_backup;
        }
    }
    mlt_sampler.iter--;
}
float calc_mis_weight(int s, int t, const in PathVertex s_fwd_pdf) {
#define cam_vtx(i) camera_verts.d[bdpt_path_idx + i]
#define light_vtx(i) light_verts.d[bdpt_path_idx + i]
#define remap0(i) (i != 0. ? i : 1.)
    bool s_0_changed = false;
    float s_0_pdf;
    vec3 s_0_pdf_pos;
    vec3 s_0_pdf_nrm;
    bool t_0_changed = false;
    uint idx_1 = -1;
    float idx_1_val;
    uint idx_2 = -1;
    float idx_2_val;
    uint idx_3 = -1;
    float idx_3_val;
    uint idx_4 = -1;
    float idx_4_val;
    uint delta_t_old;
    uint delta_s_old;
    if (s + t == 2) {
        return 1.0;
    }
    if (s == 1) {
        s_0_pdf = light_vtx(0).pdf_fwd;
        s_0_pdf_pos = light_vtx(0).pos;
        s_0_pdf_nrm = light_vtx(0).n_s;
        light_vtx(0).pdf_fwd = s_fwd_pdf.pdf_fwd;
        light_vtx(0).pos = s_fwd_pdf.pos;
        light_vtx(0).n_s = s_fwd_pdf.n_s;
        s_0_changed = true;
    }
    if (t == 1) {
        s_0_pdf = cam_vtx(0).pdf_fwd;
        s_0_pdf_pos = cam_vtx(0).pos;
        s_0_pdf_nrm = cam_vtx(0).n_s;
        cam_vtx(0).pdf_fwd = s_fwd_pdf.pdf_fwd;
        cam_vtx(0).pos = s_fwd_pdf.pos;
        cam_vtx(0).n_s = s_fwd_pdf.n_s;
        t_0_changed = true;
    }
    if (t > 0) {
        idx_1_val = cam_vtx(t - 1).pdf_rev;
        idx_1 = t;
        delta_t_old = cam_vtx(t - 1).delta;
        cam_vtx(t - 1).delta = 0;
        if (s > 0) {
            vec3 dir = (cam_vtx(t - 1).pos - light_vtx(s - 1).pos);
            float dir_len = length(dir);
            dir /= dir_len;
            const Material mat = load_material(
                light_vtx(s - 1).material_idx, light_vtx(s - 1).uv);
            vec3 wo = normalize(light_vtx(s - 2).pos - light_vtx(s - 1).pos);
            float pdf_rev;
            if (s >= 2) {
                wo = normalize(light_vtx(s - 2).pos - light_vtx(s - 1).pos);
                pdf_rev = bsdf_pdf(mat, light_vtx(s - 1).n_s, wo, dir);
            }
            if (s < 2) {
                const Light light = lights[light_vtx(s - 1).light_idx];
                pdf_rev = light_pdf(light, light_vtx(s - 1).n_s, dir);
            }
            cam_vtx(t - 1).pdf_rev = pdf_rev;
            if (cam_vtx(t - 1).pdf_rev != 0) {
                cam_vtx(t - 1).pdf_rev *=
                    abs(dot(dir, cam_vtx(t - 1).n_s)) /
                    (dir_len * dir_len);
            }
        } else {
            cam_vtx(t - 1).pdf_rev =
                1.0 / (pc_ray.light_triangle_count * cam_vtx(t - 1).area);
        }
    }
    if (t > 1) {
        idx_2_val = cam_vtx(t - 2).pdf_rev;
        idx_2 = t;
        vec3 dir = (cam_vtx(t - 2).pos - cam_vtx(t - 1).pos);
        float dir_len = length(dir);
        dir /= dir_len;
        if (s > 0) {
            const Material mat =
                load_material(cam_vtx(t - 1).material_idx, cam_vtx(t - 1).uv);
            vec3 wo = normalize(light_vtx(s - 1).pos - cam_vtx(t - 1).pos);
            cam_vtx(t - 2).pdf_rev =
                bsdf_pdf(mat, cam_vtx(t - 1).n_s, wo, dir);
            if (cam_vtx(t - 2).pdf_rev != 0) {
                cam_vtx(t - 2).pdf_rev *=
                    abs(dot(dir, cam_vtx(t - 2).n_s)) /
                    (dir_len * dir_len);
            }
        } else {
            // Assumption: All lights are finite
            float cos_x = dot(cam_vtx(t - 1).n_s, dir);
            float cos_y = dot(cam_vtx(t - 2).n_s, dir);
            cam_vtx(t - 2).pdf_rev =
                abs(cos_x * cos_y) / (PI * dir_len * dir_len);
        }
    }
    if (s > 0) {
        // t is always > 0
        idx_3_val = light_vtx(s - 1).pdf_rev;
        idx_3 = s;
        delta_s_old = light_vtx(s - 1).delta;
        light_vtx(s - 1).delta = 0;
        vec3 dir = (light_vtx(s - 1).pos - cam_vtx(t - 1).pos);
        float dir_len = length(dir);
        dir /= dir_len;
        if (t == 1) {
            float cos_theta = dot(cam_vtx(0).n_s, dir);
            float pdf = 1.0 / (cam_vtx(0).area * screen_size * cos_theta *
                               cos_theta * cos_theta);
            pdf *= abs(dot(dir, light_vtx(s - 1).n_s)) /
                   (dir_len * dir_len);
            light_vtx(s - 1).pdf_rev = pdf;
        } else {
            vec3 wo = normalize(cam_vtx(t - 2).pos - cam_vtx(t - 1).pos);
            const Material mat =
                load_material(cam_vtx(t - 1).material_idx, cam_vtx(t - 1).uv);
            light_vtx(s - 1).pdf_rev =
                bsdf_pdf(mat, cam_vtx(t - 1).n_s, wo, dir);
            if (light_vtx(s - 1).pdf_rev != 0) {
                light_vtx(s - 1).pdf_rev *=
                    abs(dot(dir, light_vtx(s - 1).n_s)) /
                    (dir_len * dir_len);
            }
        }
    }
    if (s > 1) {
        idx_4_val = light_vtx(s - 2).pdf_rev;
        idx_4 = s;
        vec3 dir = (light_vtx(s - 2).pos - light_vtx(s - 1).pos);
        vec3 wo = normalize(cam_vtx(t - 1).pos - light_vtx(s - 1).pos);
        float dir_len = length(dir);
        dir /= dir_len;
        const Material mat =
            load_material(light_vtx(s - 1).material_idx, light_vtx(s - 1).uv);
        light_vtx(s - 2).pdf_rev =
            bsdf_pdf(mat, light_vtx(s - 1).n_s, wo, dir);
        if (light_vtx(s - 2).pdf_rev != 0) {
            light_vtx(s - 2).pdf_rev *=
                abs(dot(dir, light_vtx(s - 2).n_s)) /
                (dir_len * dir_len);
        }
    }
    float sum_ri = 0.;
    float weight = 1.0;
    for (int i = t - 1; i > 0; i--) {
        weight *= remap0(cam_vtx(i).pdf_rev) / remap0(cam_vtx(i).pdf_fwd);
        if (cam_vtx(i).delta == 0 && cam_vtx(i - 1).delta == 0) {
            sum_ri += weight;
        }
    }
    weight = 1.0;
    for (int i = s - 1; i >= 0; i--) {
        weight *= remap0(light_vtx(i).pdf_rev) / remap0(light_vtx(i).pdf_fwd);
        bool delta_prev = i > 0 ? light_vtx(i - 1).delta == 1 : false;
        if (light_vtx(i).delta == 0 && !delta_prev) {
            sum_ri += weight;
        }
    }

    if (s_0_changed) {
        light_vtx(0).pdf_fwd = s_0_pdf;
        light_vtx(0).pos = s_0_pdf_pos;
        light_vtx(0).n_s = s_0_pdf_nrm;
    }
    if (t_0_changed) {
        cam_vtx(0).pdf_fwd = s_0_pdf;
        cam_vtx(0).pos = s_0_pdf_pos;
        cam_vtx(0).n_s = s_0_pdf_nrm;
    }
    if (idx_1 != -1) {
        cam_vtx(idx_1 - 1).pdf_rev = idx_1_val;
        cam_vtx(idx_1 - 1).delta = delta_t_old;
    }
    if (idx_2 != -1) {
        cam_vtx(idx_2 - 2).pdf_rev = idx_2_val;
    }
    if (idx_3 != -1) {
        light_vtx(idx_3 - 1).pdf_rev = idx_3_val;
        light_vtx(idx_3 - 1).delta = delta_s_old;
    }
    if (idx_4 != -1) {
        light_vtx(idx_4 - 2).pdf_rev = idx_4_val;
    }
#undef cam_vtx
#undef light_vtx
    return 1 / (1 + sum_ri);
}

vec3 bdpt_connect_cam(int s, out ivec2 coords) {
#define cam_vtx(i) camera_verts.d[bdpt_path_idx + i]
#define light_vtx(i) light_verts.d[bdpt_path_idx + i]
    PathVertex sampled;
    vec3 throughput = vec3(1.0);
    vec3 L = vec3(0);
    vec3 dir = cam_vtx(0).pos - light_vtx(s - 1).pos;
    float len = length(dir);
    dir /= len;
    float cos_y = dot(dir, light_vtx(s - 1).n_s);
    float cos_theta = dot(cam_vtx(0).n_s, -dir);
    if (cos_theta <= 0.) {
        return vec3(0);
    }
    float cos_3_theta = cos_theta * cos_theta * cos_theta;
    const float cam_pdf_ratio =
        abs(cos_y) / (cam_vtx(0).area * cos_3_theta * len * len);

    vec3 ray_origin =
        offset_ray(light_vtx(s - 1).pos, light_vtx(s - 1).n_s);
    const Material mat =
        load_material(light_vtx(s - 1).material_idx, light_vtx(s - 1).uv);
    const vec3 wo = normalize(light_vtx(s - 2).pos - light_vtx(s - 1).pos);
    const vec3 f = eval_bsdf(mat, wo, dir, light_vtx(s - 1).n_s);
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
            sampled.pos = cam_vtx(0).pos;
            sampled.n_s = cam_vtx(0).n_s;
            // We / pdf_we * abs(cos_theta) = cam_pdf_ratio
            L = light_vtx(s - 1).throughput * cam_pdf_ratio * f / screen_size;
        }
    }
    dir = -dir;
    vec4 target = ubo.view * vec4(dir.x, dir.y, dir.z, 0);
    target /= target.z;
    target = -ubo.projection * target;
    coords =
        ivec2(0.5 * (1 + target.xy) * vec2(pc_ray.size_x, pc_ray.size_y) - 0.5);
    if (coords.x < 0 || coords.x >= pc_ray.size_x || coords.y < 0 ||
        coords.y >= pc_ray.size_y || dot(dir, cam_vtx(0).n_s) < 0) {
        return vec3(0);
    }
    float mis_weight = 1.0;
    if (luminance(L) != 0.) {
        mis_weight = calc_mis_weight(s, 1, sampled);
    }
    return mis_weight * L;
#undef cam_vtx
#undef light_vtx
}

int mlt_random_walk(const int max_depth, in vec3 throughput, const float pdf,
                    const int type, bool large_step, inout uvec4 seed) {
#define vtx(i, prop)                                                           \
    (type == 0 ? light_verts.d[bdpt_path_idx + i + 1].prop                     \
               : camera_verts.d[bdpt_path_idx + i + 1].prop)
#define vtx_assign(i, prop, expr)                                              \
    type == 0 ? light_verts.d[bdpt_path_idx + i + 1].prop = (expr)             \
              : camera_verts.d[bdpt_path_idx + i + 1].prop = (expr)

    if (max_depth == 0)
        return 0;
    int b = 0;
    int prev = 0;
    const uint flags = gl_RayFlagsOpaqueEXT;
    const float tmin = 0.001;
    const float tmax = 10000.0;
    vec3 ray_pos = vtx(-1, pos);
    float pdf_fwd = pdf;
    float pdf_rev = 0.;
    vec3 wi = vtx(-1, dir);
    while (true) {
        prev = b - 1;
        traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, ray_pos, tmin, wi, tmax, 0);
        if (payload.material_idx == -1) {
            if (type == 1) {
                vtx_assign(b, throughput, throughput);
                vtx_assign(b, pdf_fwd, pdf_fwd);
                b++;
            }
            break;
        }
        vec3 wo = vtx(prev, pos) - payload.pos;
        float wo_len = length(wo);
        wo /= wo_len;
        vec3 n_s = payload.n_s;
        bool side = true;
        if (dot(wo, n_s) < 0) {
            n_s *= -1;
            side = false;
        }
        vtx_assign(b, pdf_fwd,
                   pdf_fwd * abs(dot(wo, n_s)) / (wo_len * wo_len));
        vtx_assign(b, n_s, n_s);
        vtx_assign(b, area, payload.area);
        vtx_assign(b, pos, payload.pos);
        vtx_assign(b, uv, payload.uv);
        vtx_assign(b, material_idx, payload.material_idx);
        vtx_assign(b, throughput, throughput);
        const Material mat =
            load_material(payload.material_idx, payload.uv);
        const bool mat_specular =
            (mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR;
        const bool mat_transmissive =
            (mat.bsdf_props & BSDF_TRANSMISSIVE) == BSDF_TRANSMISSIVE;
        vtx_assign(b, delta, int(mat_specular));

        if (++b >= max_depth) {
            break;
        }
        float cos_theta;
        vec2 rands_dir =
            vec2(mlt_rand(seed, large_step), mlt_rand(seed, large_step));
        const vec3 f = sample_bsdf(n_s, wo, mat, type, side, wi,
                                   pdf_fwd, cos_theta, rands_dir);
        const bool same_hem = same_hemisphere(wi, wo, n_s);
        if (f == vec3(0) || pdf_fwd == 0 || (!same_hem && !mat_transmissive)) {
            break;
        }
        throughput *= f * abs(cos_theta) / pdf_fwd;
        pdf_rev = pdf_fwd;
        if (!mat_specular) {
            pdf_rev = bsdf_pdf(mat, n_s, wi, wo);
        }
        if (pdf_rev > 0.) {
            pdf_rev *= abs(dot(vtx(prev, n_s), wo)) / (wo_len * wo_len);
        }
        vtx_assign(prev, pdf_rev, pdf_rev);
        ray_pos = offset_ray(payload.pos, n_s);
    }
#undef vtx
#undef vtx_assign
    return b;
}

int mlt_generate_light_subpath(int max_depth, bool large_step,
                               inout uvec4 seed) {
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
    // const TriangleRecord record =
    //     sample_area_light(rands_pos, pc_ray.num_lights, light_idx,
    //                       light_triangle_idx, light_material_idx, light);
    // const Material light_mat =
    //     load_material(light_material_idx, uv_unused);
     if (pdf_dir <= 0) {
        return 0;
    }
    const vec3 Le = sample_light_Le(
        rands_pos, rands_dir, pc_ray.num_lights, pc_ray.light_triangle_count,
        light_idx, light_triangle_idx, light_material_idx, light, record,
        light_mat, pdf_pos, seed, wi, pdf_dir);
    light_verts.d[bdpt_path_idx].pos = record.pos;
    light_verts.d[bdpt_path_idx].n_s = record.n_s;
    light_verts.d[bdpt_path_idx].material_idx = light_material_idx;
    light_verts.d[bdpt_path_idx].light_idx = light_idx;
    light_verts.d[bdpt_path_idx].area = 1.0 / record.triangle_pdf;
    light_verts.d[bdpt_path_idx].delta = 0;
    light_verts.d[bdpt_path_idx].dir = wi;
    light_verts.d[bdpt_path_idx].pdf_fwd = pdf_pos;
    light_verts.d[bdpt_path_idx].throughput = Le;
    vec3 throughput = Le * abs(dot(record.n_s, wi)) /
                      (pdf_dir * light_verts.d[bdpt_path_idx].pdf_fwd);

    return mlt_random_walk(max_depth - 1, throughput, pdf_dir, 0, large_step,
                           seed) +
           1;
}

int mlt_generate_camera_subpath(const vec3 origin, int max_depth,
                                const float cam_area, bool large_step,
                                inout uvec4 seed) {
    camera_verts.d[bdpt_path_idx].pos = origin;
    // Use n_g as camera direction
    vec2 d =
        vec2(mlt_rand(seed, large_step), mlt_rand(seed, large_step)) * 2.0 -
        1.0;
    camera_verts.d[bdpt_path_idx].dir = vec3(sample_camera(d));
    camera_verts.d[bdpt_path_idx].area = cam_area;
    camera_verts.d[bdpt_path_idx].throughput = vec3(1.0);
    camera_verts.d[bdpt_path_idx].delta = 0;
    camera_verts.d[bdpt_path_idx].n_s =
        vec3(-ubo.inv_view * vec4(0, 0, 1, 0));
    float cos_theta = dot(camera_verts.d[bdpt_path_idx].dir,
                          camera_verts.d[bdpt_path_idx].n_s);
    float pdf =
        1 / (cam_area * screen_size * cos_theta * cos_theta * cos_theta);
    ivec2 coords = ivec2(0.5 * (1 + d) * vec2(pc_ray.size_x, pc_ray.size_y));
    camera_verts.d[bdpt_path_idx].coords = coords.x * pc_ray.size_y + coords.y;
    return mlt_random_walk(max_depth - 1, vec3(1), pdf, 1, large_step, seed) +
           1;
}

vec3 mlt_connect(int s, int t, bool large_step, inout uvec4 seed,
                 bool save_radiance) {
#define cam_vtx(i) camera_verts.d[bdpt_path_idx + i]
#define light_vtx(i) light_verts.d[bdpt_path_idx + i]
#define splat(i) splat_data.d[splat_idx + i]
#define mlt_sampler mlt_samplers.d[mlt_sampler_idx]
    vec3 L = vec3(0);
    PathVertex s_fwd_pdf;
    if (s == 0) {
        // Pure camera path
        uint mat_idx = cam_vtx(t - 1).material_idx;
        Material mat = materials.m[mat_idx];
        if (mat_idx != -1) {
            L = mat.emissive_factor * cam_vtx(t - 1).throughput;
        }
    } else if (s == 1) {
        // Connect from camera path to light(aka NEE)
        uint light_idx;
        uint light_triangle_idx;
        uint light_material_idx;
        vec2 uv_unused;
        Light light;
        TriangleRecord record;
        Material light_mat;
        const vec4 rands_pos =
            vec4(mlt_rand(seed, large_step), mlt_rand(seed, large_step),
                 mlt_rand(seed, large_step), mlt_rand(seed, large_step));
        // const TriangleRecord record =
        //     sample_area_light(rands_pos, pc_ray.num_lights, light_idx,
        //                       light_triangle_idx, light_material_idx, light);
        // const Material light_mat =
        //     load_material(light_material_idx, uv_unused);
        const vec3 Le =
            sample_light(cam_vtx(t - 1).pos, pc_ray.num_lights, light_idx,
                         light_triangle_idx, light_material_idx, light, record,
                         light_mat, seed);
        vec3 wi = record.pos - cam_vtx(t - 1).pos;
        float ray_len = length(wi);
        float ray_len_sqr = ray_len * ray_len;
        wi /= ray_len;
        const float cos_x = abs(dot(wi, cam_vtx(t - 1).n_s));
        const vec3 ray_origin =
            offset_ray(cam_vtx(t - 1).pos, cam_vtx(t - 1).n_s);
        any_hit_payload.hit = 1;
        vec3 wo = normalize(cam_vtx(t - 2).pos - cam_vtx(t - 1).pos);
        // TODO
        const Material mat =
            load_material(cam_vtx(t - 1).material_idx, cam_vtx(t - 1).uv);
        const vec3 f = eval_bsdf(mat, wo, wi, cam_vtx(t - 1).n_s);
        if (f != vec3(0)) {
            traceRayEXT(tlas,
                        gl_RayFlagsTerminateOnFirstHitEXT |
                            gl_RayFlagsSkipClosestHitShaderEXT,
                        0xFF, 1, 0, 1, ray_origin, 0, wi, ray_len - EPS, 1);
            const bool visible = any_hit_payload.hit == 0;
            if (visible) {
                float cos_y = abs(dot(-wi, record.n_s));
                float G = abs(cos_x * cos_y) / ray_len_sqr;
                float pdf_light =
                    record.triangle_pdf / pc_ray.light_triangle_count;

                s_fwd_pdf.pdf_fwd = pdf_light;
                s_fwd_pdf.pos = record.pos;
                s_fwd_pdf.n_s = record.n_s;
                vec3 throughput = Le / pdf_light;
                L = cam_vtx(t - 1).throughput * f * G * throughput;
            }
        }
    } else {
        // Eval G
        vec3 n_s = light_vtx(s - 1).n_s;
        vec3 n_t = cam_vtx(t - 1).n_s;
        vec3 d = light_vtx(s - 1).pos - cam_vtx(t - 1).pos;
        float len = length(d);
        d /= len;
        float G = (dot(n_s, -d)) * (dot(n_t, d)) / (len * len);
        if (G > 0) {
            const Material mat_1 =
                load_material(cam_vtx(t - 1).material_idx, cam_vtx(t - 1).uv);
            const Material mat_2 = load_material(
                light_vtx(s - 1).material_idx, light_vtx(s - 1).uv);

            vec3 wo_1 = normalize(cam_vtx(t - 2).pos - cam_vtx(t - 1).pos);
            vec3 wo_2 = normalize(light_vtx(s - 2).pos - light_vtx(s - 1).pos);
            const vec3 brdf1 =
                eval_bsdf(mat_1, wo_1, d, cam_vtx(t - 1).n_s);
            const vec3 brdf2 =
                eval_bsdf(mat_2, wo_2, -d, light_vtx(t - 1).n_s);
            if (brdf1 != vec3(0) && brdf2 != vec3(0)) {
                vec3 ray_origin =
                    offset_ray(cam_vtx(t - 1).pos, cam_vtx(t - 1).n_s);
                // Check visibility
                any_hit_payload.hit = 1;
                traceRayEXT(tlas,
                            gl_RayFlagsTerminateOnFirstHitEXT |
                                gl_RayFlagsSkipClosestHitShaderEXT,
                            0xFF, 1, 0, 1, ray_origin, 0, d, len - EPS, 1);
                const bool visible = any_hit_payload.hit == 0;
                if (visible) {
                    L = light_vtx(s - 1).throughput * G * brdf1 * brdf2 *
                        cam_vtx(t - 1).throughput;
                }
            }
        }
    }

    float mis_weight = 1.0f;
    if (luminance(L) != 0.) {
        mis_weight = calc_mis_weight(s, t, s_fwd_pdf);
        L *= mis_weight;
    }
    return L;
#undef cam_vtx
#undef light_vtx
#undef splat
#undef mlt_sampler
}

float mlt_L(const vec4 origin, const float cam_area, bool large_step,
            inout uvec4 seed, const bool save_radiance) {
#define mlt_sampler mlt_samplers.d[mlt_sampler_idx]
#define splat(i) splat_data.d[splat_idx + i]
    float lum_sum = 0;
    if (save_radiance) {
        mlt_start_chain(0);
    }
    int num_light_paths =
        mlt_generate_light_subpath(pc_ray.max_depth + 1, large_step, seed);
    if (save_radiance) {
        mlt_start_chain(1);
    }
    int num_cam_paths = mlt_generate_camera_subpath(
        origin.xyz, pc_ray.max_depth + 1, cam_area, large_step, seed);
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
                L += mlt_connect(s, t, large_step, seed, save_radiance);
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