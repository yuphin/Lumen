#ifndef BDPT_COMMONS
#define BDPT_COMMONS

#ifndef BDPT_MLT
#define BDPT_MLT 0
#endif

float light_pdf_pos;
#if BDPT_MLT == 1
#include "mlt_commons.glsl"
#endif

int bdpt_random_walk_light(const int max_depth, vec3 throughput,
                           const float pdf) {
#define vtx(i, prop) light_verts.d[bdpt_path_idx + i + 1].prop
#define vtx_assign(i, prop, expr)                                              \
    light_verts.d[bdpt_path_idx + i + 1].prop = (expr)
    if (max_depth == 0)
        return 0;
    int b = 0;
    int prev = 0;
    const uint flags = gl_RayFlagsOpaqueEXT;
    const float tmin = 0.001;
    const float tmax = 1e6;
    vec3 ray_pos = vtx(-1, pos);
    float pdf_fwd = pdf;
    float pdf_rev = 0.;
    vec3 wi = vtx(-1, dir);
    bool finite_light = is_light_finite(vtx(-1, light_flags));
    while (true) {
        prev = b - 1;
        traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, ray_pos, tmin, wi, tmax, 0);
        if (payload.material_idx == -1) {
            break;
        }

        vec3 wo = vtx(prev, pos) - payload.pos;
        float wo_len = length(wo);
        wo /= wo_len;
        vec3 n_s = payload.n_s;
        bool side = true;
        vec3 n_g = payload.n_g;
        if (dot(payload.n_g, wo) < 0.)
            n_g = -n_g;
        if (dot(n_g, n_s) < 0) {
            n_s *= -1;
            side = false;
        }
        vtx_assign(b, pdf_fwd, pdf_fwd * abs(dot(wo, n_s)) / (wo_len * wo_len));
        vtx_assign(b, n_s, n_s);
        vtx_assign(b, area, payload.area);
        vtx_assign(b, pos, payload.pos);
        vtx_assign(b, uv, payload.uv);
        vtx_assign(b, material_idx, payload.material_idx);
        vtx_assign(b, throughput, throughput);
        const Material mat = load_material(payload.material_idx, payload.uv);
        const bool mat_specular =
            (mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR;
        const bool mat_transmissive =
            (mat.bsdf_props & BSDF_TRANSMISSIVE) == BSDF_TRANSMISSIVE;
        vtx_assign(b, delta, int(mat_specular));

        if (++b >= max_depth) {
            break;
        }

        float cos_theta;
#if BDPT_MLT == 1
        vec2 rands_dir = vec2(mlt_rand(mlt_seed, large_step),
                              mlt_rand(mlt_seed, large_step));
        const vec3 f = sample_bsdf(n_s, wo, mat, 0, side, wi, pdf_fwd,
                                   cos_theta, rands_dir);
#else
        const vec3 f =
            sample_bsdf(n_s, wo, mat, 0, side, wi, pdf_fwd, cos_theta, seed);
#endif
        const bool same_hem = same_hemisphere(wi, wo, n_s);
        if (f == vec3(0) || pdf_fwd == 0 || (!same_hem && !mat_transmissive)) {
            break;
        }
        throughput *= f * abs(cos_theta) / pdf_fwd;

        pdf_rev = pdf_fwd;
        if (!mat_specular) {
            pdf_rev = bsdf_pdf(mat, n_s, wi, wo);
        }
        bool g_term = false;
        if (prev > -1 || finite_light) {
            g_term = true;
        }
        if (g_term) {
            pdf_rev *= abs(dot(vtx(prev, n_s), wo)) / (wo_len * wo_len);
        }
        vtx_assign(prev, pdf_rev, pdf_rev);
        ray_pos = offset_ray(payload.pos, n_g);
    }
#undef vtx
#undef vtx_assign
    return b;
}

int bdpt_random_walk_eye(const int max_depth, vec3 throughput,
                         const float pdf) {
#define vtx(i, prop) camera_verts.d[bdpt_path_idx + i + 1].prop
#define vtx_assign(i, prop, expr)                                              \
    camera_verts.d[bdpt_path_idx + i + 1].prop = (expr)
    if (max_depth == 0)
        return 0;
    int b = 0;
    int prev = 0;
    const uint flags = gl_RayFlagsOpaqueEXT;
    const float tmin = 0.001;
    const float tmax = 1e6;
    vec3 ray_pos = vtx(-1, pos);
    float pdf_fwd = pdf;
    float pdf_rev = 0.;
    vec3 wi = vtx(-1, dir);
    bool finite_light = is_light_finite(vtx(-1, light_flags));
    while (true) {
        prev = b - 1;
        traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, ray_pos, tmin, wi, tmax, 0);
        if (payload.material_idx == -1) {
            vtx_assign(b, throughput, throughput);
            vtx_assign(b, pdf_fwd, pdf_fwd);
            b++;
            break;
        }

        vec3 wo = vtx(prev, pos) - payload.pos;
        float wo_len = length(wo);
        wo /= wo_len;
        vec3 n_s = payload.n_s;
        bool side = true;
        vec3 n_g = payload.n_g;
        if (dot(payload.n_g, wo) < 0.)
            n_g = -n_g;
        if (dot(n_g, n_s) < 0) {
            n_s *= -1;
            side = false;
        }
        vtx_assign(b, pdf_fwd, pdf_fwd * abs(dot(wo, n_s)) / (wo_len * wo_len));
        vtx_assign(b, n_s, n_s);
        vtx_assign(b, area, payload.area);
        vtx_assign(b, pos, payload.pos);
        vtx_assign(b, uv, payload.uv);
        vtx_assign(b, material_idx, payload.material_idx);
        vtx_assign(b, throughput, throughput);
        const Material mat = load_material(payload.material_idx, payload.uv);
        const bool mat_specular =
            (mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR;
        const bool mat_transmissive =
            (mat.bsdf_props & BSDF_TRANSMISSIVE) == BSDF_TRANSMISSIVE;
        vtx_assign(b, delta, int(mat_specular));

        if (++b >= max_depth) {
            break;
        }

        float cos_theta;
#if BDPT_MLT == 1
        vec2 rands_dir = vec2(mlt_rand(mlt_seed, large_step),
                              mlt_rand(mlt_seed, large_step));
        const vec3 f = sample_bsdf(n_s, wo, mat, 1, side, wi, pdf_fwd,
                                   cos_theta, rands_dir);
#else
        const vec3 f =
            sample_bsdf(n_s, wo, mat, 1, side, wi, pdf_fwd, cos_theta, seed);
#endif

        const bool same_hem = same_hemisphere(wi, wo, n_s);
        if (f == vec3(0) || pdf_fwd == 0 || (!same_hem && !mat_transmissive)) {
            break;
        }
        throughput *= f * abs(cos_theta) / pdf_fwd;

        pdf_rev = pdf_fwd;
        if (!mat_specular) {
            pdf_rev = bsdf_pdf(mat, n_s, wi, wo);
        }
        bool g_term = true;

        if (g_term) {
            pdf_rev *= abs(dot(vtx(prev, n_s), wo)) / (wo_len * wo_len);
        }
        vtx_assign(prev, pdf_rev, pdf_rev);
        ray_pos = offset_ray(payload.pos, n_g);
    }
#undef vtx
#undef vtx_assign
    return b;
}

int bdpt_generate_light_subpath(int max_depth) {
    LightRecord light_record;
    vec3 wi, pos, n;
    float pdf_pos, pdf_dir;
    float cos_theta;
#if BDPT_MLT == 1
    const vec4 rands_pos =
        vec4(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step),
             mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
    const vec2 rands_dir =
        vec2(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
    const vec3 Le = sample_light_Le(
        pc.num_lights, pc.light_triangle_count, cos_theta, light_record,
        pos, wi, n, pdf_pos, pdf_dir, rands_pos, rands_dir);
#else
    const vec3 Le =
        sample_light_Le(seed, pc.num_lights, pc.light_triangle_count,
                        cos_theta, light_record, pos, wi, n, pdf_pos, pdf_dir);
#endif
    if (pdf_dir <= 0) {
        return 0;
    }
    light_pdf_pos = pdf_pos;

    light_verts.d[bdpt_path_idx].pos = pos;
    light_verts.d[bdpt_path_idx].light_flags = light_record.flags;
    light_verts.d[bdpt_path_idx].delta = 0;
    light_verts.d[bdpt_path_idx].dir = wi;
    light_verts.d[bdpt_path_idx].pdf_fwd = pdf_pos;
    light_verts.d[bdpt_path_idx].n_s = n;
    vec3 throughput =
        Le * cos_theta / (pdf_dir * light_verts.d[bdpt_path_idx + 0].pdf_fwd);
    light_verts.d[bdpt_path_idx + 0].throughput = Le;
    int num_light_verts =
        bdpt_random_walk_light(max_depth - 1, throughput, pdf_dir) + 1;
    if (!is_light_finite(light_record.flags)) {
        light_verts.d[bdpt_path_idx + 1].pdf_fwd =
            pdf_pos * abs(dot(wi, light_verts.d[bdpt_path_idx + 1].n_s));
    }
    if (is_light_delta(light_record.flags)) {
        light_verts.d[bdpt_path_idx].pdf_fwd = 0;
    }
    return num_light_verts;
}

int bdpt_generate_camera_subpath(vec2 d, const vec3 origin, int max_depth,
                                 const float cam_area) {
#if BDPT_MLT == 1
    d = vec2(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step)) *
            2.0 -
        1.0;
#endif
    camera_verts.d[bdpt_path_idx].pos = origin;
    camera_verts.d[bdpt_path_idx].dir = vec3(sample_camera(d));
    camera_verts.d[bdpt_path_idx].area = cam_area;
    camera_verts.d[bdpt_path_idx].throughput = vec3(1.0);
    camera_verts.d[bdpt_path_idx].delta = 0;
    camera_verts.d[bdpt_path_idx].n_s = vec3(-ubo.inv_view * vec4(0, 0, 1, 0));
#if BDPT_MLT == 1
    ivec2 coords = ivec2(0.5 * (1 + d) * vec2(pc.size_x, pc.size_y));
    camera_verts.d[bdpt_path_idx].coords = coords.x * pc.size_y + coords.y;
#endif
    float cos_theta = dot(camera_verts.d[bdpt_path_idx].dir,
                          camera_verts.d[bdpt_path_idx].n_s);
    float pdf =
        1 / (cam_area * screen_size * cos_theta * cos_theta * cos_theta);
    return bdpt_random_walk_eye(max_depth - 1, vec3(1), pdf) + 1;
}

float calc_mis_weight(int s, int t, const in PathVertex sampled) {
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
        light_vtx(0).pdf_fwd = sampled.pdf_fwd;
        light_vtx(0).pos = sampled.pos;
        light_vtx(0).n_s = sampled.n_s;
        s_0_changed = true;
    }
    if (t == 1) {
        s_0_pdf = cam_vtx(0).pdf_fwd;
        s_0_pdf_pos = cam_vtx(0).pos;
        s_0_pdf_nrm = cam_vtx(0).n_s;
        cam_vtx(0).pdf_fwd = sampled.pdf_fwd;
        cam_vtx(0).pos = sampled.pos;
        cam_vtx(0).n_s = sampled.n_s;
        t_0_changed = true;
    }
    if (t > 0) {
        delta_t_old = cam_vtx(t - 1).delta;
        cam_vtx(t - 1).delta = 0;
    }
    if (s > 0) {
        delta_s_old = light_vtx(s - 1).delta;
        light_vtx(s - 1).delta = 0;
    }

    if (t > 0) {
        idx_1_val = cam_vtx(t - 1).pdf_rev;
        idx_1 = t;

        if (s > 0) {
            vec3 dir = (cam_vtx(t - 1).pos - light_vtx(s - 1).pos);
            float dir_len = length(dir);
            dir /= dir_len;
            const Material mat = load_material(light_vtx(s - 1).material_idx,
                                               light_vtx(s - 1).uv);
            vec3 wo = normalize(light_vtx(s - 2).pos - light_vtx(s - 1).pos);
            float pdf_rev;
            if (s >= 2) {
                wo = normalize(light_vtx(s - 2).pos - light_vtx(s - 1).pos);
                pdf_rev = bsdf_pdf(mat, light_vtx(s - 1).n_s, wo, dir);
                pdf_rev *=
                    abs(dot(dir, cam_vtx(t - 1).n_s)) / (dir_len * dir_len);
            } else if (s == 1) {
                if (!is_light_finite(light_vtx(0).light_flags)) {
                    // Note: All the infinite lights are of directional type
                    pdf_rev = light_pdf_pos;
                    pdf_rev *= abs(dot(dir, cam_vtx(t - 1).n_s));
                } else {
                    pdf_rev = light_pdf(light_vtx(0).light_flags,
                                        light_vtx(0).n_s, dir);
                    pdf_rev *=
                        abs(dot(dir, cam_vtx(t - 1).n_s)) / (dir_len * dir_len);
                }
            }
            cam_vtx(t - 1).pdf_rev = pdf_rev;
        } else {
            // s == 0, i.e the path is on a finite light source
            // cam_vtx(t-1).area gives the area of the emitter that was hit
            cam_vtx(t - 1).pdf_rev =
                1.0 / (pc.light_triangle_count * cam_vtx(t - 1).area);
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
            cam_vtx(t - 2).pdf_rev = bsdf_pdf(mat, cam_vtx(t - 1).n_s, wo, dir);
            if (cam_vtx(t - 2).pdf_rev != 0) {
                cam_vtx(t - 2).pdf_rev *=
                    abs(dot(dir, cam_vtx(t - 2).n_s)) / (dir_len * dir_len);
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
        vec3 dir = (light_vtx(s - 1).pos - cam_vtx(t - 1).pos);
        float dir_len = length(dir);
        dir /= dir_len;
        if (t == 1) {
            // t == 1 implies s > 1
            float cos_theta = dot(cam_vtx(0).n_s, dir);
            float pdf = 1.0 / (cam_vtx(0).area * screen_size * cos_theta *
                               cos_theta * cos_theta);
            pdf *= abs(dot(dir, light_vtx(s - 1).n_s)) / (dir_len * dir_len);
            light_vtx(s - 1).pdf_rev = pdf;
        } else {
            vec3 wo = normalize(cam_vtx(t - 2).pos - cam_vtx(t - 1).pos);
            const Material mat =
                load_material(cam_vtx(t - 1).material_idx, cam_vtx(t - 1).uv);
            light_vtx(s - 1).pdf_rev =
                bsdf_pdf(mat, cam_vtx(t - 1).n_s, wo, dir);
            if ((s == 1 && is_light_finite(light_vtx(0).light_flags)) ||
                s > 1) {
                light_vtx(s - 1).pdf_rev *=
                    abs(dot(dir, light_vtx(s - 1).n_s)) / (dir_len * dir_len);
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
        // t - 1 -> s-1 -> s-2
        light_vtx(s - 2).pdf_rev = bsdf_pdf(mat, light_vtx(s - 1).n_s, wo, dir);
        // g = 1 for infinite lights
        if (s == 2 && is_light_finite(light_vtx(0).light_flags) || s > 2) {
            light_vtx(s - 2).pdf_rev *=
                abs(dot(dir, light_vtx(s - 2).n_s)) / (dir_len * dir_len);
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
        bool delta_prev = i > 0 ? light_vtx(i - 1).delta == 1
                                : is_light_delta(light_vtx(0).light_flags);
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

    vec3 ray_origin = offset_ray2(light_vtx(s - 1).pos, light_vtx(s - 1).n_s);
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
        ivec2(0.5 * (1 + target.xy) * vec2(pc.size_x, pc.size_y) - 0.5);
    if (coords.x < 0 || coords.x >= pc.size_x || coords.y < 0 ||
        coords.y >= pc.size_y || dot(dir, cam_vtx(0).n_s) < 0) {
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

vec3 bdpt_connect(int s, int t) {
#define cam_vtx(i) camera_verts.d[bdpt_path_idx + i]
#define light_vtx(i) light_verts.d[bdpt_path_idx + i]
    vec3 L = vec3(0);
    PathVertex sampled;
    if (s == 0) {
        // Pure camera path
        uint mat_idx = cam_vtx(t - 1).material_idx;
        Material mat = materials.m[mat_idx];
        if (mat_idx != -1) {
            L = mat.emissive_factor * cam_vtx(t - 1).throughput;
        } else {
            L = vec3(1, 1, 1) * cam_vtx(t - 1).throughput;
        }
    } else if (s == 1) {
        vec3 wi;
        float wi_len;
        float pdf_pos_a;
        vec3 n;
        vec3 pos;
        LightRecord record;
        float cos_y;
#if BDPT_MLT == 1
        const vec4 rands_pos = vec4(
            mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step),
            mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
        const vec3 Le =
            sample_light_Li(rands_pos, cam_vtx(t - 1).pos, pc.num_lights,
                            wi, wi_len, n, pos, pdf_pos_a, cos_y, record);
#else
        const vec3 Le =
            sample_light_Li(seed, cam_vtx(t - 1).pos, pc.num_lights, wi,
                            wi_len, n, pos, pdf_pos_a, cos_y, record);
#endif
        const float cos_x = abs(dot(wi, cam_vtx(t - 1).n_s));
        const vec3 ray_origin =
            offset_ray2(cam_vtx(t - 1).pos, cam_vtx(t - 1).n_s);
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
                        0xFF, 1, 0, 1, ray_origin, 0, wi, wi_len - EPS, 1);
            const bool visible = any_hit_payload.hit == 0;
            if (visible) {
                const float pdf_light_w =
                    light_pdf_a_to_w(record.flags, pdf_pos_a, n,
                                     wi_len * wi_len, cos_y) /
                    pc.light_triangle_count;
                sampled.pdf_fwd = pdf_pos_a / pc.light_triangle_count;
                sampled.pos = pos;
                sampled.n_s = n;
                sampled.delta = uint(is_light_delta(record.flags));
                L = cam_vtx(t - 1).throughput * f * abs(cos_x) * Le /
                    pdf_light_w;
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
            const Material mat_2 = load_material(light_vtx(s - 1).material_idx,
                                                 light_vtx(s - 1).uv);

            vec3 wo_1 = normalize(cam_vtx(t - 2).pos - cam_vtx(t - 1).pos);
            vec3 wo_2 = normalize(light_vtx(s - 2).pos - light_vtx(s - 1).pos);
            const vec3 brdf1 = eval_bsdf(mat_1, wo_1, d, cam_vtx(t - 1).n_s);
            const vec3 brdf2 = eval_bsdf(mat_2, wo_2, -d, light_vtx(s - 1).n_s);
            if (brdf1 != vec3(0) && brdf2 != vec3(0)) {
                vec3 ray_origin =
                    offset_ray2(cam_vtx(t - 1).pos, cam_vtx(t - 1).n_s);
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
#undef cam_vtx
#undef light_vtx
    float mis_weight = 1.0f;
    if (luminance(L) != 0.) {
        mis_weight = calc_mis_weight(s, t, sampled);

        L *= mis_weight;
    }
    return vec3(L);
}
#endif