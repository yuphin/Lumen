#ifndef VCM_COMMONS
#define VCM_COMMONS

#ifndef VC_MLT
#define VC_MLT 0
#endif
#ifndef VCM_MLT
#define VCM_MLT 0
#endif

#if VC_MLT == 1
#include "mlt_commons.glsl"
#elif VCM_MLT == 1
#include "mlt2_commons.glsl"
#endif

vec3 normalize_grid(vec3 p, vec3 min_bnds, vec3 max_bnds) {
    return (p - min_bnds) / (max_bnds - min_bnds);
}

ivec3 get_grid_idx(vec3 p, vec3 min_bnds, vec3 max_bnds, ivec3 grid_res) {
    ivec3 res = ivec3(normalize_grid(p, min_bnds, max_bnds) * grid_res);
    clamp(res, vec3(0), grid_res - vec3(1));
    return res;
}
vec3 vcm_connect_cam(const vec3 cam_pos, const vec3 cam_nrm, vec3 n_s,
                     const float cam_A, const vec3 pos, const in VCMState state,
                     const float eta_vm, const vec3 wo, const Material mat,
                     out ivec2 coords) {
    vec3 L = vec3(0);
    vec3 dir = cam_pos - pos;
    float len = length(dir);
    dir /= len;
    float cos_y = dot(dir, n_s);
    float cos_theta = dot(cam_nrm, -dir);
    if (cos_theta <= 0.) {
        return L;
    }

    // if(dot(n_s, dir) < 0) {
    //     n_s *= -1;
    // }
    // pdf_rev / pdf_fwd
    // in the case of light coming to camera
    // simplifies to abs(cos(theta)) / (A * cos^3(theta) * len^2)
    float cos_3_theta = cos_theta * cos_theta * cos_theta;
    const float cam_pdf_ratio = abs(cos_y) / (cam_A * cos_3_theta * len * len);
    vec3 ray_origin = offset_ray2(pos, n_s);
    float pdf_rev, pdf_fwd;
    const vec3 f = eval_bsdf(n_s, wo, mat, 0, dot(payload.n_s, wo) > 0, dir,
                             pdf_fwd, pdf_rev, cos_y);
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
            const float w_light = (cam_pdf_ratio / (screen_size)) *
                                  (eta_vm + state.d_vcm + pdf_rev * state.d_vc);

            const float mis_weight = 1. / (1. + w_light);
            // We / pdf_we * abs(cos_theta) = cam_pdf_ratio
            L = mis_weight * state.throughput * cam_pdf_ratio * f / screen_size;
            // if(isnan(luminance(L))) {
            //     debugPrintfEXT("%v3f\n", state.throughput);
            // }
        }
    }
    dir = -dir;
    vec4 target = ubo.view * vec4(dir.x, dir.y, dir.z, 0);
    target /= target.z;
    target = -ubo.projection * target;
    vec2 screen_dims = vec2(pc.size_x, pc.size_y);
    coords = ivec2(0.5 * (1 + target.xy) * screen_dims - 0.5);
    if (coords.x < 0 || coords.x >= pc.size_x || coords.y < 0 ||
        coords.y >= pc.size_y || dot(dir, cam_nrm) < 0) {
        return vec3(0);
    }
    return L;
}

bool vcm_generate_light_sample(float eta_vc, out VCMState light_state,
                               out bool finite) {
    // Sample light
    uint light_idx;
    uint light_triangle_idx;
    uint light_material_idx;
    vec2 uv_unused;
    LightRecord light_record;
    vec3 wi, pos;
    float pdf_pos, pdf_dir, pdf_emit, pdf_direct;
    float cos_theta;
#if VC_MLT == 1 || VCM_MLT == 1
    const vec4 rands_pos =
        vec4(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step),
             mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
    const vec2 rands_dir =
        vec2(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
    const vec3 Le =
        sample_light_Le(rands_pos, rands_dir, pc.num_lights,
                        pc.light_triangle_count, cos_theta, light_record,
                        pos, wi, pdf_pos, pdf_dir, pdf_emit, pdf_direct);
#else
    const vec3 Le = sample_light_Le(
        seed, pc.num_lights, pc.light_triangle_count, cos_theta,
        light_record, pos, wi, pdf_pos, pdf_dir, pdf_emit, pdf_direct);
#endif
    if (pdf_dir <= 0) {
        return false;
    }
    light_state.pos = pos;
    light_state.area = 1.0 / pdf_pos;
    light_state.wi = wi;
    light_state.throughput = Le * cos_theta / (pdf_dir * pdf_pos);

    // Partially evaluate pdfs (area formulation)
    // At s = 0 this is p_rev / p_fwd, in the case of area lights:
    // p_rev = p_connect = 1/area, p_fwd = cos_theta / (PI * area)
    // Note that pdf_fwd is in area formulation, so cos_y / r^2 is missing
    // currently.
    light_state.d_vcm = pdf_direct / pdf_emit;
    // g_prev / p_fwd
    // Note that g_prev component in d_vc and d_vm lags by 1 iter
    // So we initialize g_prev to cos_theta of the current iter
    // Also note that 1/r^2 in the geometry term cancels for vc and vm
    // By convention pdf_fwd sample the i'th vertex from i-1
    // g_prev or pdf_prev samples from i'th vertex to i-1
    // In that sense, cos_theta terms will be common in g_prev and pdf_pwd
    // Similar argument, with the eta
    finite = is_light_finite(light_record.flags);
    if (!is_light_delta(light_record.flags)) {
        light_state.d_vc = (finite ? cos_theta : 1) / (pdf_dir * pdf_pos);
    } else {
        light_state.d_vc = 0;
    }
    light_state.d_vm = light_state.d_vc * eta_vc;
    return true;
}

vec3 vcm_get_light_radiance(in const Material mat,
                            in const VCMState camera_state, int d) {
    if (d == 1) {
        return mat.emissive_factor;
    }
    const float pdf_light_pos =
        1.0 / (payload.area * pc.light_triangle_count);

    const float pdf_light_dir = abs(dot(payload.n_s, -camera_state.wi)) / PI;
    const float w_camera =
        pdf_light_pos * camera_state.d_vcm +
        (pc.use_vc == 1 || pc.use_vm == 1
             ? (pdf_light_pos * pdf_light_dir) * camera_state.d_vc
             : 0);
    const float mis_weight = 1. / (1. + w_camera);
    return mis_weight * mat.emissive_factor;
}

vec3 vcm_connect_light(vec3 n_s, vec3 wo, Material mat, bool side, float eta_vm,
                       VCMState camera_state, out float pdf_rev, out vec3 f) {
    vec3 wi;
    float wi_len;
    float pdf_pos_w;
    float pdf_pos_dir_w;
    LightRecord record;
    float cos_y;
    vec3 res = vec3(0);
#if VC_MLT == 1
    const vec4 rands_pos =
        vec4(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step),
             mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
    const vec3 Le =
        sample_light_Li(rands_pos, payload.pos, pc.num_lights, wi, wi_len,
                        pdf_pos_w, pdf_pos_dir_w, cos_y, record);
#elif VCM_MLT == 1
    vec3 Le;
    if (SEEDING == 1) {
        Le =
            sample_light_Li(seed, payload.pos, pc.num_lights, wi, wi_len,
                            pdf_pos_w, pdf_pos_dir_w, record, cos_y);
    } else {
        const vec4 rands_pos = vec4(
            mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step),
            mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
        Le =
            sample_light_Li(rands_pos, payload.pos, pc.num_lights, wi,
                            wi_len, pdf_pos_w, pdf_pos_dir_w, cos_y, record);
    }
#else
    const vec3 Le =
        sample_light_Li(seed, payload.pos, pc.num_lights, wi, wi_len,
                        pdf_pos_w, pdf_pos_dir_w, record, cos_y);
#endif

    const float cos_x = dot(wi, n_s);
    const vec3 ray_origin = offset_ray2(payload.pos, n_s);
    any_hit_payload.hit = 1;
    float pdf_fwd;
    f = eval_bsdf(n_s, wo, mat, 1, side, wi, pdf_fwd, pdf_rev, cos_x);
    if (f != vec3(0)) {
        traceRayEXT(tlas,
                    gl_RayFlagsTerminateOnFirstHitEXT |
                        gl_RayFlagsSkipClosestHitShaderEXT,
                    0xFF, 1, 0, 1, ray_origin, 0, wi, wi_len - EPS, 1);
        const bool visible = any_hit_payload.hit == 0;
        if (visible) {
            if (is_light_delta(record.flags)) {
                pdf_fwd = 0;
            }
            const float w_light =
                pdf_fwd / (pdf_pos_w / pc.light_triangle_count);
            const float w_cam =
                pdf_pos_dir_w * abs(cos_x) / (pdf_pos_w * cos_y) *
                (eta_vm + camera_state.d_vcm + camera_state.d_vc * pdf_rev);
            const float mis_weight = 1. / (1. + w_light + w_cam);
            if (mis_weight > 0) {
                res = mis_weight * abs(cos_x) * f * camera_state.throughput *
                      Le / (pdf_pos_w / pc.light_triangle_count);
            }
        }
    }
    return res;
}

#define light_vtx(i) vcm_lights.d[i]
vec3 vcm_connect_light_vertices(uint light_path_len, uint light_path_idx,
                                int depth, vec3 n_s, vec3 wo, Material mat,
                                bool side, float eta_vm, VCMState camera_state,
                                float pdf_rev) {
    vec3 res = vec3(0);
    for (int i = 0; i < light_path_len; i++) {
        uint s = light_vtx(light_path_idx + i).path_len;
        uint mdepth = s + depth - 1;
        if (mdepth >= pc.max_depth) {
            break;
        }
        vec3 dir = light_vtx(light_path_idx + i).pos - payload.pos;
        const float len = length(dir);
        const float len_sqr = len * len;
        dir /= len;
        const float cos_cam = dot(n_s, dir);
        const float cos_light = dot(light_vtx(light_path_idx + i).n_s, -dir);
        const float G = cos_light * cos_cam / len_sqr;
        if (G > 0) {
            float cam_pdf_fwd, light_pdf_fwd, light_pdf_rev;
            const vec3 f_cam =
                eval_bsdf(n_s, wo, mat, 1, side, dir, cam_pdf_fwd, cos_cam);
            const Material light_mat =
                load_material(light_vtx(light_path_idx + i).material_idx,
                              light_vtx(light_path_idx + i).uv);
            // TODO: what about anisotropic BSDFS?
            const vec3 f_light =
                eval_bsdf(light_vtx(light_path_idx + i).n_s,
                          light_vtx(light_path_idx + i).wo, light_mat, 0,
                          light_vtx(light_path_idx + i).side == 1, -dir,
                          light_pdf_fwd, light_pdf_rev, cos_light);
            if (f_light != vec3(0) && f_cam != vec3(0)) {
                cam_pdf_fwd *= abs(cos_light) / len_sqr;
                light_pdf_fwd *= abs(cos_cam) / len_sqr;
                const float w_light =
                    cam_pdf_fwd *
                    (eta_vm + light_vtx(light_path_idx + i).d_vcm +
                     light_pdf_rev * light_vtx(light_path_idx + i).d_vc);
                const float w_camera =
                    light_pdf_fwd *
                    (eta_vm + camera_state.d_vcm + pdf_rev * camera_state.d_vc);
                const float mis_weight = 1. / (1 + w_camera + w_light);
                const vec3 ray_origin = offset_ray2(payload.pos, n_s);
                any_hit_payload.hit = 1;
                traceRayEXT(tlas,
                            gl_RayFlagsTerminateOnFirstHitEXT |
                                gl_RayFlagsSkipClosestHitShaderEXT,
                            0xFF, 1, 0, 1, ray_origin, 0, dir, len - EPS, 1);
                const bool visible = any_hit_payload.hit == 0;
                if (visible) {
                    res = mis_weight * G * camera_state.throughput *
                          light_vtx(light_path_idx + i).throughput * f_cam *
                          f_light;
                }
            }
        }
    }
    return res;
}
#undef light_vtx

#if VC_MLT == 0
vec3 vcm_merge_light_vertices(uint light_path_len, uint light_path_idx,
                              int depth, vec3 n_s, vec3 wo, Material mat,
                              bool side, float eta_vc, VCMState camera_state,
                              float pdf_rev, float radius,
                              float normalization_factor) {
    float r_sqr = radius * radius;
    vec3 res = vec3(0);
    ivec3 grid_min_bnds_idx =
        get_grid_idx(payload.pos - vec3(radius), pc.min_bounds,
                     pc.max_bounds, pc.grid_res);
    ivec3 grid_max_bnds_idx =
        get_grid_idx(payload.pos + vec3(radius), pc.min_bounds,
                     pc.max_bounds, pc.grid_res);
    for (int x = grid_min_bnds_idx.x; x <= grid_max_bnds_idx.x; x++) {
        for (int y = grid_min_bnds_idx.y; y <= grid_max_bnds_idx.y; y++) {
            for (int z = grid_min_bnds_idx.z; z <= grid_max_bnds_idx.z; z++) {
                const uint h = hash(ivec3(x, y, z), screen_size);
                if (photons.d[h].photon_count > 0) {
                    const vec3 pp = payload.pos - photons.d[h].pos;
                    const float dist_sqr = dot(pp, pp);
                    if (dist_sqr > r_sqr) {
                        continue;
                    }
                    // Should we?
                    uint depth = photons.d[h].path_len + depth - 1;
                    if (depth > pc.max_depth) {
                        continue;
                    }
                    float cam_pdf_fwd, cam_pdf_rev;
                    const float cos_theta = dot(photons.d[h].wi, n_s);
                    vec3 f = eval_bsdf(n_s, wo, mat, 1, side, photons.d[h].wi,
                                       cam_pdf_fwd, cam_pdf_rev, cos_theta);

                    if (f != vec3(0)) {
                        const float w_light = photons.d[h].d_vcm * eta_vc +
                                              photons.d[h].d_vm * cam_pdf_fwd;
                        const float w_cam = camera_state.d_vcm * eta_vc +
                                            camera_state.d_vm * cam_pdf_rev;

                        const float mis_weight = 1. / (1 + w_light + w_cam);
                        float cos_nrm = dot(photons.d[h].nrm, n_s);
                        if (cos_nrm > EPS) {
                            const float w = 1. - sqrt(dist_sqr) / radius;
                            const float w_normalization =
                                3.; // 1. / (1 - 2/(3*k)) where k =
                                    // 1
                            res = w * mis_weight * photons.d[h].photon_count *
                                  photons.d[h].throughput * f *
                                  camera_state.throughput *
                                  normalization_factor * w_normalization;
                        }
                    }
                }
            }
        }
    }
    return res;
}
#endif

#if VC_MLT == 1 || VCM_MLT == 1
float vcm_fill_light(vec3 origin, VCMState vcm_state, bool finite_light,
#else
void vcm_fill_light(vec3 origin, VCMState vcm_state, bool finite_light,
#endif
                     float eta_vcm, float eta_vc, float eta_vm) {
#define light_vtx(i) vcm_lights.d[vcm_light_path_idx + i]
    const vec3 cam_pos = origin;
    const vec3 cam_nrm = vec3(-ubo.inv_view * vec4(0, 0, 1, 0));
    const float radius = pc.radius;
    const float radius_sqr = radius * radius;
    vec4 area_int = (ubo.inv_projection * vec4(2. / gl_LaunchSizeEXT.x,
                                               2. / gl_LaunchSizeEXT.y, 0, 1));
    area_int /= (area_int.w);
    const float cam_area = abs(area_int.x * area_int.y);
    int depth;
    int path_idx = 0;
    bool specular = false;
#if VC_MLT == 1 || VCM_MLT == 1
    float lum_sum = 0;
#endif
    light_vtx(path_idx).path_len = 0;
    for (depth = 1;; depth++) {
        traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, vcm_state.pos, tmin,
                    vcm_state.wi, tmax, 0);
        if (payload.material_idx == -1) {
            break;
        }
        vec3 wo = vcm_state.pos - payload.pos;

        vec3 n_s = payload.n_s;
        vec3 n_g = payload.n_g;
        bool side = true;
        if (dot(payload.n_g, wo) <= 0.)
            n_g = -n_g;
        if (dot(n_g, n_s) < 0) {
            n_s = -n_s;
            side = false;
        }
        float cos_wo = dot(wo, n_s);
        float dist = length(payload.pos - vcm_state.pos);
        float dist_sqr = dist * dist;
        wo /= dist;
        const Material mat = load_material(payload.material_idx, payload.uv);
        const bool mat_specular =
            (mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR;
        // Complete the missing geometry terms
        float cos_theta_wo = abs(dot(wo, n_s));

        if (depth > 1 || finite_light) {
            vcm_state.d_vcm *= dist_sqr;
        }
        vcm_state.d_vcm /= cos_theta_wo;
        vcm_state.d_vc /= cos_theta_wo;
        vcm_state.d_vm /= cos_theta_wo;
        if ((!mat_specular && (pc.use_vc == 1 || pc.use_vm == 1))) {

            // Copy to light vertex buffer
            // light_vtx(path_idx).wi = vcm_state.wi;
            light_vtx(path_idx).wo = wo; //-vcm_state.wi;
            light_vtx(path_idx).n_s = n_s;
            light_vtx(path_idx).pos = payload.pos;
            light_vtx(path_idx).uv = payload.uv;
            light_vtx(path_idx).material_idx = payload.material_idx;
            light_vtx(path_idx).area = payload.area;
            light_vtx(path_idx).throughput = vcm_state.throughput;
            light_vtx(path_idx).d_vcm = vcm_state.d_vcm;
            light_vtx(path_idx).d_vc = vcm_state.d_vc;
            light_vtx(path_idx).d_vm = vcm_state.d_vm;
            light_vtx(path_idx).path_len = depth + 1;
            light_vtx(path_idx).side = uint(side);
            path_idx++;
        }
        if (depth >= pc.max_depth) {
            break;
        }
        // Reverse pdf in solid angle form, since we have geometry term
        // at the outer paranthesis
        if (!mat_specular && (pc.use_vc == 1 && depth < pc.max_depth)) {
            // Connect to camera
            ivec2 coords;
            vec3 splat_col =
                vcm_connect_cam(cam_pos, cam_nrm, n_s, cam_area, payload.pos,
                                vcm_state, eta_vm, wo, mat, coords);
#if VC_MLT == 1
#define splat(i) light_splats.d[splat_idx + i]
            const float lum = luminance(splat_col);
            if (save_radiance && lum > 0) {
                connected_lights.d[pixel_idx]++;

                uint idx = coords.x * pc.size_y + coords.y;
                const uint splat_cnt = light_splat_cnts.d[pixel_idx];
                light_splat_cnts.d[pixel_idx]++;
                splat(splat_cnt).idx = idx;
                splat(splat_cnt).L = splat_col;
            } else if (lum > 0) {
                connected_lights.d[pixel_idx]++;
                lum_sum += lum;
            }
#undef splat

#elif VCM_MLT == 1
            const float lum = luminance(splat_col);
            if (lum > 0) {
                lum_sum += lum;
                uint idx = coords.x * pc.size_y + coords.y;
                tmp_col.d[idx] += splat_col;
            }
#else
                                 if (luminance(splat_col) > 0) {
                                     uint idx = coords.x * gl_LaunchSizeEXT.y +
                                                coords.y;
                                     tmp_col.d[idx] += splat_col;
                                 }
#endif
        }

        // Continue the walk
        float pdf_dir;
        float cos_theta;
#if VC_MLT == 1 || VCM_MLT == 1
        vec2 rands_dir = vec2(mlt_rand(mlt_seed, large_step),
                              mlt_rand(mlt_seed, large_step));
        const vec3 f = sample_bsdf(n_s, wo, mat, 0, side, vcm_state.wi, pdf_dir,
                                   cos_theta, rands_dir);
#else
        const vec3 f = sample_bsdf(n_s, wo, mat, 0, side, vcm_state.wi, pdf_dir,
                                   cos_theta, seed);
#endif

        const bool same_hemisphere = same_hemisphere(vcm_state.wi, wo, n_s);

        const bool mat_transmissive =
            (mat.bsdf_props & BSDF_TRANSMISSIVE) == BSDF_TRANSMISSIVE;
        if (f == vec3(0) || pdf_dir == 0 ||
            (!same_hemisphere && !mat_transmissive)) {
            break;
        }
        float pdf_rev = pdf_dir;
        if (!mat_specular) {
            pdf_rev = bsdf_pdf(mat, n_s, vcm_state.wi, wo);
        }
        const float abs_cos_theta = abs(cos_theta);

        vcm_state.pos = offset_ray(payload.pos, n_g);
        // Note, same cancellations also occur here from now on
        // see _vcm_generate_light_sample_
        if (!mat_specular) {
            vcm_state.d_vc =
                (abs_cos_theta / pdf_dir) *
                (eta_vm + vcm_state.d_vcm + pdf_rev * vcm_state.d_vc);
            vcm_state.d_vm =
                (abs_cos_theta / pdf_dir) *
                (1 + vcm_state.d_vcm * eta_vc + pdf_rev * vcm_state.d_vm);
            vcm_state.d_vcm = 1.0 / pdf_dir;
        } else {
            // Specular pdf has value = inf, so d_vcm = 0;
            vcm_state.d_vcm = 0;
            // pdf_fwd = pdf_rev = delta -> cancels
            vcm_state.d_vc *= abs_cos_theta;
            vcm_state.d_vm *= abs_cos_theta;
            specular = true;
        }
        vcm_state.throughput *= f * abs_cos_theta / pdf_dir;
        vcm_state.n_s = n_s;
        vcm_state.area = payload.area;
        vcm_state.material_idx = payload.material_idx;
    }
    light_path_cnts.d[pixel_idx] = path_idx;
    // "Build" the hash grid
    // TODO: Add sorting later
#if VC_MLT == 0
    if (pc.use_vm == 1) {
        for (int i = 0; i < path_idx; i++) {
            ivec3 grid_idx = get_grid_idx(light_vtx(i).pos, pc.min_bounds,
                                          pc.max_bounds, pc.grid_res);
            uint h = hash(grid_idx, screen_size);
            photons.d[h].pos = light_vtx(i).pos;
            photons.d[h].wi = -light_vtx(i).wi;
            photons.d[h].d_vm = light_vtx(i).d_vm;
            photons.d[h].d_vcm = light_vtx(i).d_vcm;
            photons.d[h].throughput = light_vtx(i).throughput;
            photons.d[h].nrm = light_vtx(i).n_s;
            photons.d[h].path_len = light_vtx(i).path_len;
            atomicAdd(photons.d[h].photon_count, 1);
        }
    }
#endif
#if VC_MLT == 1 || VCM_MLT == 1
    return lum_sum;
#endif
}
#undef light_vtx

vec3 vcm_trace_eye(VCMState camera_state, float eta_vcm, float eta_vc,
#if VC_MLT == 1 || VCM_MLT == 1
                   float eta_vm, out float lum) {
#define splat(i) splat_data.d[splat_idx + i]
    lum = 0;
#else
                   float eta_vm) {
#endif
    float avg_len = 0;
    uint cnt = 1;
    const float radius = pc.radius;
    const float radius_sqr = radius * radius;

#if VC_MLT == 1
    uint light_path_idx = uint(mlt_rand(mlt_seed, large_step) * screen_size);
    uint light_splat_idx =
        light_path_idx * pc.max_depth * (pc.max_depth + 1);
    uint light_path_len = light_path_cnts.d[light_path_idx];
    mlt_sampler.splat_cnt = 0;
    if (save_radiance && connected_lights.d[pixel_idx] > 0) {
        const uint light_splat_cnt = light_splat_cnts.d[pixel_idx];
        for (int i = 0; i < light_splat_cnt; i++) {
            const vec3 light_L = light_splats.d[light_splat_idx + i].L;
            lum += luminance(light_L);
            const uint splat_cnt = mlt_sampler.splat_cnt;
            mlt_sampler.splat_cnt++;
            splat(splat_cnt).idx = light_splats.d[light_splat_idx + i].idx;
            splat(splat_cnt).L = light_L;
        }
    } else if (connected_lights.d[pixel_idx] > 0) {
        lum += tmp_lum_data.d[pixel_idx];
    }
    light_path_idx *= (pc.max_depth + 1);
    light_splat_cnts.d[pixel_idx] = 0;
#elif VCM_MLT == 1
    const uint num_light_paths = pc.size_x * pc.size_y;
    uint light_path_idx = uint(mlt_rand(seed, large_step) * num_light_paths);
    uint light_splat_idx =
        light_path_idx * pc.max_depth * (pc.max_depth + 1);
    uint light_path_len = light_path_cnts.d[light_path_idx];
    mlt_sampler.splat_cnt = 0;
    light_path_idx *= (pc.max_depth + 1);
#else
    uint light_path_idx = uint(rand(seed) * screen_size);
    uint light_path_len = light_path_cnts.d[light_path_idx];
    light_path_idx *= (pc.max_depth + 1);
#endif
    vec3 col = vec3(0);
    int depth;
    const float normalization_factor = 1. / (PI * radius_sqr * screen_size);

    for (depth = 1;; depth++) {
        traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, camera_state.pos, tmin,
                    camera_state.wi, tmax, 0);

        if (payload.material_idx == -1) {
            // TODO:
            col += camera_state.throughput * pc.sky_col;
            break;
        }
        vec3 wo = camera_state.pos - payload.pos;
        float dist = length(payload.pos - camera_state.pos);
        float dist_sqr = dist * dist;
        wo /= dist;
        vec3 n_s = payload.n_s;
        vec3 n_g = payload.n_g;
        bool side = true;
        if (dot(payload.n_g, wo) < 0.)
            n_g = -n_g;
        if (dot(n_g, n_s) < 0) {
            n_s = -n_s;
            side = false;
        }
        float cos_wo = abs(dot(wo, n_s));

        const Material mat = load_material(payload.material_idx, payload.uv);
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
                   vcm_get_light_radiance(mat, camera_state, depth);
            // if (pc.use_vc == 1 || pc.use_vm == 1) {
            //     // break;
            // }
        }
        // Connect to light
        float pdf_rev;
        vec3 f;
        if (!mat_specular && depth < pc.max_depth) {
            col += vcm_connect_light(n_s, wo, mat, side, eta_vm, camera_state,
                                     pdf_rev, f);
        }

        // Connect to light vertices
        if (!mat_specular) {
            col += vcm_connect_light_vertices(light_path_len, light_path_idx,
                                              depth, n_s, wo, mat, side, eta_vm,
                                              camera_state, pdf_rev);
        }
#if VC_MLT == 0
        // Vertex merging
        float r_sqr = radius * radius;
        if (!mat_specular && pc.use_vm == 1) {
            col += vcm_merge_light_vertices(
                light_path_len, light_path_idx, depth, n_s, wo, mat, side,
                eta_vc, camera_state, pdf_rev, radius, normalization_factor);
        }
#endif
        if (depth >= pc.max_depth) {
            break;
        }

        // Scattering
        float pdf_dir;
        float cos_theta;
#if VC_MLT == 1 || VCM_MLT == 1
        vec2 rands_dir = vec2(mlt_rand(mlt_seed, large_step),
                              mlt_rand(mlt_seed, large_step));
        f = sample_bsdf(n_s, wo, mat, 0, side, camera_state.wi, pdf_dir,
                        cos_theta, rands_dir);
#else
        f = sample_bsdf(n_s, wo, mat, 1, side, camera_state.wi, pdf_dir,
                        cos_theta, seed);
#endif

        const bool mat_transmissive =
            (mat.bsdf_props & BSDF_TRANSMISSIVE) == BSDF_TRANSMISSIVE;
        const bool same_hemisphere = same_hemisphere(camera_state.wi, wo, n_s);
        if (f == vec3(0) || pdf_dir == 0 ||
            (!same_hemisphere && !mat_transmissive)) {
            break;
        }
        pdf_rev = pdf_dir;
        if (!mat_specular) {
            pdf_rev = bsdf_pdf(mat, n_s, camera_state.wi, wo);
        }
        const float abs_cos_theta = abs(cos_theta);

        camera_state.pos = offset_ray(payload.pos, n_g);
        // Note, same cancellations also occur here from now on
        // see _vcm_generate_light_sample_
        if (!mat_specular) {
            camera_state.d_vc =
                ((abs_cos_theta) / pdf_dir) *
                (eta_vm + camera_state.d_vcm + pdf_rev * camera_state.d_vc);
            camera_state.d_vm =
                ((abs_cos_theta) / pdf_dir) *
                (1 + camera_state.d_vcm * eta_vc + pdf_rev * camera_state.d_vm);
            camera_state.d_vcm = 1.0 / pdf_dir;
        } else {
            camera_state.d_vcm = 0;
            camera_state.d_vc *= abs_cos_theta;
            camera_state.d_vm *= abs_cos_theta;
        }

        camera_state.throughput *= f * abs_cos_theta / pdf_dir;
        camera_state.n_s = n_s;
        camera_state.area = payload.area;
        cnt++;
    }

#undef splat
    return col;
}

#if VCM_MLT == 1
float mlt_fill_eye() {
#define cam_vtx(i) vcm_lights.d[vcm_light_path_idx + i]
    const float fov = ubo.projection[1][1];
    vec3 cam_pos = vec3(ubo.inv_view * vec4(0, 0, 0, 1));
    vec4 area_int = (ubo.inv_projection * vec4(2. / gl_LaunchSizeEXT.x,
                                               2. / gl_LaunchSizeEXT.y, 0, 1));
    area_int /= area_int.w;
    const float cam_area = abs(area_int.x * area_int.y);
    vec2 dir = vec2(rand(seed), rand(seed)) * 2.0 - 1.0;
    const vec3 direction = sample_camera(dir).xyz;
    VCMState camera_state;
    float lum_sum = 0;
    // Generate camera sample
    camera_state.wi = direction;
    camera_state.pos = cam_pos;
    camera_state.throughput = vec3(1.0);
    camera_state.n_s = vec3(-ubo.inv_view * vec4(0, 0, 1, 0));
    float cos_theta = abs(dot(camera_state.n_s, direction));
    // Defer r^2 / cos term
    camera_state.d_vcm = cam_area * pc.size_x * pc.size_y * cos_theta *
                         cos_theta * cos_theta;
    camera_state.d_vc = 0;
    camera_state.d_vm = 0;
    int depth;
    int path_idx = 0;
    ivec2 coords = ivec2(0.5 * (1 + dir) * vec2(pc.size_x, pc.size_y));
    uint coords_idx = coords.x * pc.size_y + coords.y;
    for (depth = 1;; depth++) {
        traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, camera_state.pos, tmin,
                    camera_state.wi, tmax, 0);

        if (payload.material_idx == -1) {
            tmp_col.d[coords_idx] += camera_state.throughput * pc.sky_col;
            break;
        }

        vec3 wo = camera_state.pos - payload.pos;
        float dist = length(payload.pos - camera_state.pos);
        float dist_sqr = dist * dist;
        wo /= dist;
        vec3 n_s = payload.n_s;
        float cos_wo = dot(wo, n_s);
        vec3 n_g = payload.n_g;
        bool side = true;
        if (dot(payload.n_g, wo) < 0.)
            n_g = -n_g;
        if (cos_wo < 0.) {
            cos_wo = -cos_wo;
            n_s = -n_s;
            side = false;
        }

        const Material mat = load_material(payload.material_idx, payload.uv);
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
                     vcm_get_light_radiance(mat, camera_state, depth);
            tmp_col.d[coords_idx] += L;
            lum_sum += luminance(L);
        }

        // Copy to camera vertex buffer
        if (!mat_specular) {
            cam_vtx(path_idx).wo = wo;
            cam_vtx(path_idx).n_s = n_s;
            cam_vtx(path_idx).pos = offset_ray(payload.pos, n_s);
            cam_vtx(path_idx).uv = payload.uv;
            cam_vtx(path_idx).material_idx = payload.material_idx;
            cam_vtx(path_idx).area = payload.area;
            cam_vtx(path_idx).throughput = camera_state.throughput;
            cam_vtx(path_idx).d_vcm = camera_state.d_vcm;
            cam_vtx(path_idx).d_vc = camera_state.d_vc;
            cam_vtx(path_idx).d_vm = camera_state.d_vm;
            cam_vtx(path_idx).path_len = depth + 1;
            cam_vtx(path_idx).side = uint(side);
            cam_vtx(path_idx).coords = coords_idx;
            path_idx++;
        }

        // Connect to light
        float pdf_rev;
        vec3 f;
        if (!mat_specular && depth < pc.max_depth) {
            const vec3 L = vcm_connect_light(n_s, wo, mat, side, 0,
                                             camera_state, pdf_rev, f);
            tmp_col.d[coords_idx] += L;
            lum_sum += luminance(L);
        }

        if (depth >= pc.max_depth) {
            break;
        }
        // Scattering
        float pdf_dir;
        float cos_theta;
        f = sample_bsdf(n_s, wo, mat, 0, side, camera_state.wi, pdf_dir,
                        cos_theta, seed);

        const bool mat_transmissive =
            (mat.bsdf_props & BSDF_TRANSMISSIVE) == BSDF_TRANSMISSIVE;
        const bool same_hemisphere = same_hemisphere(camera_state.wi, wo, n_s);
        if (f == vec3(0) || pdf_dir == 0 ||
            (!same_hemisphere && !mat_transmissive)) {
            break;
        }
        pdf_rev = pdf_dir;
        if (!mat_specular) {
            pdf_rev = bsdf_pdf(mat, n_s, camera_state.wi, wo);
        }
        const float abs_cos_theta = abs(cos_theta);

        camera_state.pos = offset_ray(payload.pos, n_s);
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
        camera_state.n_s = n_s;
        camera_state.area = payload.area;
        camera_state.material_idx = payload.material_idx;
    }
    light_path_cnts.d[pixel_idx] = path_idx;
#undef cam_vtx
    return lum_sum;
}

float mlt_trace_light() {
#define splat(i) splat_data.d[splat_idx + chain * depth_factor + i]
    vec3 cam_pos = vec3(ubo.inv_view * vec4(0, 0, 0, 1));
    vec4 area_int = (ubo.inv_projection * vec4(2. / gl_LaunchSizeEXT.x,
                                               2. / gl_LaunchSizeEXT.y, 0, 1));
    area_int /= area_int.w;
    const float cam_area = abs(area_int.x * area_int.y);
    vec3 cam_nrm = vec3(-ubo.inv_view * vec4(0, 0, 1, 0));
    // Select camera path
    float luminance_sum = 0;
    mlt_sampler.splat_cnt = 0;
    uint path_idx =
        uint(mlt_rand(mlt_seed, large_step) * (pc.size_x * pc.size_y));
    uint path_len = light_path_cnts.d[path_idx];
    path_idx *= (pc.max_depth + 1);
    // Trace from light
    VCMState light_state;
    bool finite;
    if (!vcm_generate_light_sample(0, light_state, finite)) {
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

        vec3 n_s = payload.n_s;
        float cos_wo = dot(wo, n_s);
        vec3 n_g = payload.n_g;
        bool side = true;
        if (dot(payload.n_g, wo) <= 0.)
            n_g = -n_g;
        if (cos_wo <= 0.) {
            cos_wo = -cos_wo;
            n_s = -n_s;
            side = false;
        }

        if (dot(n_g, wo) * dot(n_s, wo) <= 0) {
            // We dont handle BTDF at the moment
            break;
        }
        float dist = length(payload.pos - light_state.pos);
        float dist_sqr = dist * dist;
        wo /= dist;
        const Material mat = load_material(payload.material_idx, payload.uv);
        const bool mat_specular =
            (mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR;
        // Complete the missing geometry terms
        float cos_theta_wo = abs(dot(wo, n_s));
        // Can't connect from specular to camera path, can't merge either
        if (d > 1 || finite) {
            light_state.d_vcm *= dist_sqr;
        }
        light_state.d_vcm /= cos_theta_wo;
        light_state.d_vc /= cos_theta_wo;
        light_state.d_vm /= cos_theta_wo;
        if (d >= pc.max_depth + 1) {
            break;
        }
        if (d < pc.max_depth) {
            // Connect to camera
            ivec2 coords;
            vec3 splat_col =
                vcm_connect_cam(cam_pos, cam_nrm, n_s, cam_area, payload.pos,
                                light_state, 0, wo, mat, coords);
            const float lum_val = luminance(splat_col);
            if (lum_val > 0) {
                luminance_sum += lum_val;
                if (save_radiance) {
                    const uint idx = coords.x * pc.size_y + coords.y;
                    const uint splat_cnt = mlt_sampler.splat_cnt;
                    mlt_sampler.splat_cnt++;
                    splat(splat_cnt).idx = idx;
                    splat(splat_cnt).L = splat_col;
                }
            }
        }
        vec3 unused;

        if (!mat_specular) {
#define cam_vtx(i) vcm_lights.d[i]
            // Connect to cam vertices
            for (int i = 0; i < path_len; i++) {
                uint t = cam_vtx(path_idx + i).path_len;
                uint depth = t + d - 1;
                if (depth >= pc.max_depth) {
                    break;
                }
                vec3 dir = hit_pos - cam_vtx(path_idx + i).pos;
                const float len = length(dir);
                const float len_sqr = len * len;
                dir /= len;
                const float cos_light = dot(n_s, -dir);
                const float cos_cam = dot(cam_vtx(path_idx + i).n_s, dir);
                const float G = cos_cam * cos_light / len_sqr;
                if (G > 0) {
                    float pdf_rev = bsdf_pdf(mat, n_s, -dir, wo);
                    vec3 unused;
                    float cam_pdf_fwd, cam_pdf_rev, light_pdf_fwd;
                    const Material cam_mat =
                        load_material(cam_vtx(path_idx + i).material_idx,
                                      cam_vtx(path_idx + i).uv);
                    const vec3 f_cam = eval_bsdf(
                        cam_vtx(path_idx + i).n_s, cam_vtx(path_idx + i).wo,
                        cam_mat, 1, cam_vtx(path_idx + i).side == 1, dir,
                        cam_pdf_fwd, cam_pdf_rev, cos_cam);
                    const vec3 f_light = eval_bsdf(n_s, wo, mat, 0, side, -dir,
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
                        const vec3 ray_origin = offset_ray(hit_pos, n_s);
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
        vec2 rands_dir = vec2(mlt_rand(mlt_seed, large_step),
                              mlt_rand(mlt_seed, large_step));
        const vec3 f = sample_bsdf(n_s, wo, mat, 0, side, light_state.wi,
                                   pdf_dir, cos_theta, rands_dir);
        const bool same_hemisphere = same_hemisphere(light_state.wi, wo, n_s);

        const bool mat_transmissive =
            (mat.bsdf_props & BSDF_TRANSMISSIVE) == BSDF_TRANSMISSIVE;
        if (f == vec3(0) || pdf_dir == 0 ||
            (!same_hemisphere && !mat_transmissive)) {
            break;
        }

        float pdf_rev = pdf_dir;
        if (!mat_specular) {
            pdf_rev = bsdf_pdf(mat, n_s, light_state.wi, wo);
        }
        const float abs_cos_theta = abs(cos_theta);

        light_state.pos = offset_ray(payload.pos, n_s);
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
        light_state.n_s = n_s;
        light_state.area = payload.area;
        light_state.material_idx = payload.material_idx;
    }
    return luminance_sum;
#undef splat
#undef cam_vtx
}
#endif
#endif