// #define UNIFORM_SAMPLING
Material load_material(const uint material_idx, const vec2 uv) {
    Material m = materials.m[material_idx];
    if (m.texture_id > -1) {
        m.albedo *= texture(scene_textures[m.texture_id], uv).xyz;
    }
    return m;
}

bool same_hemisphere(in vec3 wi, in vec3 wo, in vec3 n) {
    return dot(wi, n) * dot(wo, n) > 0;
}

vec3 sample_cos_hemisphere(vec2 uv, vec3 n) {
    float phi = PI2 * uv.x;
    float cos_theta = 2.0 * uv.y - 1.0;
#ifndef UNIFORM_SAMPLING
    return normalize(
        n + vec3(sqrt(1.0 - cos_theta * cos_theta) * vec2(cos(phi), sin(phi)),
                 cos_theta));
#else
    return normalize(
        vec3(sqrt(1.0 - cos_theta * cos_theta) * vec2(cos(phi), sin(phi)),
                 cos_theta));
#endif
}

vec3 sample_cos_hemisphere(vec2 uv, vec3 n, inout float phi) {
    phi = PI2 * uv.x;
    float cos_theta = 2.0 * uv.y - 1.0;
#ifndef UNIFORM_SAMPLING
    return normalize(
       n + vec3(sqrt(1.0 - cos_theta * cos_theta) * vec2(cos(phi), sin(phi)),
                 cos_theta));
#else
    return normalize(
       vec3(sqrt(1.0 - cos_theta * cos_theta) * vec2(cos(phi), sin(phi)),
                 cos_theta));
#endif
}

float diffuse_pdf(const vec3 n, const vec3 l, out float cos_theta) {
    cos_theta = dot(n, l);
#ifndef UNIFORM_SAMPLING
    return max(cos_theta / PI, 0);
#else
    return 1.0 / (2.0 * PI);
#endif
}

float diffuse_pdf(const vec3 n, const vec3 wi, const vec3 wo) { 
#ifndef UNIFORM_SAMPLING
    return max(dot(n, wi) / PI, 0); 
#else
    return 1.0 / (2.0 * PI);
#endif
}

float glossy_pdf(float cos_theta, float hl, float nh, float beckmann_term) {
    return 0.5 * (max(cos_theta / PI, 0) + beckmann_term * nh / (4 * hl));
}

float disney_pdf(const vec3 n, const Material mat, const vec3 v, const vec3 l) {
    const vec3 h = normalize(l + v);
    // VNDF pdf
    const float pdf_diffuse_lobe = 0.5 * (1 - mat.metallic);
    const float pdf_spec_lobe = 1. - pdf_diffuse_lobe;
    const float pdf_gtr2_lobe = 1. / (1. + mat.clearcoat);
    const float pdf_clearcoat_lobe = 1. - pdf_gtr2_lobe;
    float abs_nl = abs(dot(n, l));
    float abs_lh = abs(dot(h, l));
    float abs_vh = abs(dot(h, v));
    float abs_nh = abs(dot(n, h));
    float abs_nv = abs(dot(n, v));
    float alpha = max(0.00001, mat.roughness);
    float D = GTR2(abs_nh, alpha);
    float G = smithG_GGX(abs_nv, alpha);

    // Specular lobe
    // float pdf_specular = 0.25 * D * G * abs_lh / (abs_nl * abs_vh) ;
    float pdf_specular = 0.25 * D * G / abs_nv;
    // Diffuse
    float pdf_diffuse = abs_nl / PI;
    // Clearcoat
    float pdf_clearcoat = 0.25 *
                          GTR1(abs_nh, mix(0.1, 0.001, mat.clearcoat_gloss)) *
                          abs_nh / abs_vh;

    pdf_clearcoat *= (pdf_spec_lobe * pdf_clearcoat_lobe);
    pdf_diffuse *= pdf_diffuse_lobe;
    pdf_specular *= (pdf_spec_lobe * pdf_gtr2_lobe);

    return pdf_diffuse + pdf_specular + pdf_clearcoat;
}

vec3 diffuse_f(const Material mat) { return mat.albedo / PI; }

vec3 glossy_f(const Material mat, const vec3 wo, const vec3 wi, const vec3 n_s,
              float hl, float nl, float nv, float beckmann_term) {
    const vec3 h = normalize(wo + wi);
    const vec3 f_diffuse =
        (28. / (23 * PI)) * vec3(mat.albedo) * (1 - mat.metalness) *
        (1 - pow5(1 - 0.5 * dot(wi, n_s))) * (1 - pow5(1 - 0.5 * dot(wo, n_s)));
    const vec3 f_specular = beckmann_term *
                            fresnel_schlick(vec3(mat.metalness), hl) /
                            (4 * hl * max(nl, nv));
    return f_specular + f_diffuse;
}

vec3 disney_f(const Material mat, const vec3 wo, const vec3 wi,
              const vec3 n_s) {
    vec3 h = wo + wi;
    const float nl = abs(dot(n_s, wi));
    const float nv = abs(dot(n_s, wo));
    if (nl == 0 || nv == 0) {
        return vec3(0);
    }
    if (h.x == 0 && h.y == 0 && h.z == 0) {
        return vec3(0);
    }
    h = normalize(h);
    const float lh = dot(wi, h);
    const float nh = dot(n_s, h);
    const float lum = luminance(mat.albedo);
    vec3 C_tint = lum > 0.0 ? mat.albedo / lum
                            : vec3(1); // normalize lum. to isolate hue+sat
    vec3 C_spec0 =
        mix(mat.specular * 0.08 * mix(vec3(1), C_tint, mat.specular_tint),
            mat.albedo, mat.metallic);
    vec3 C_sheen = mix(vec3(1), C_tint, mat.sheen_tint);
    // Diffuse (F_d)
    float F_l = schlick_w(nl);
    float F_v = schlick_w(nv);
    float F_d90 = 0.5 + 2 * lh * lh * mat.roughness;
    float F_d = mix(1.0, F_d90, F_l) * mix(1.0, F_d90, F_v);
    // Isotropic BSSRDF
    float F_ss90 = lh * lh * mat.roughness;
    float F_ss = mix(1.0, F_ss90, F_l) * mix(1.0, F_ss90, F_v);
    float ss = 1.25 * (F_ss * (1.0 / (nl + nv) - 0.5) + 0.5);
    // Specular (f_s) (only isotropic for now)
    float alpha = max(0.001, mat.roughness);
    float D_s = GTR2(nh, alpha);
    float fh = schlick_w(lh);
    vec3 F_s = mix(C_spec0, vec3(1), fh);
    float alpha_g = sqr(0.5 + 0.5 * alpha);
    float G_s = smithG_GGX(nl, alpha_g) * smithG_GGX(nv, alpha_g);
    // Sheen (f_sheen)
    vec3 F_sheen = mat.sheen * C_sheen * fh;
    // Clearcoat
    float D_r = GTR1(nh, mix(0.1, 0.001, mat.clearcoat_gloss));
    float F_r = mix(0.04, 1.0, fh);
    float G_r = smithG_GGX(nl, .25) * smithG_GGX(nv, .25);
    return ((1 / PI) * mix(F_d, ss, mat.subsurface) * mat.albedo + F_sheen) *
               (1 - mat.metallic) +
           0.25 * G_s * F_s * D_s / (nl * nv) +
           0.25 * mat.clearcoat * G_r * F_r * D_r;
}

vec3 sample_bsdf(const vec3 n_s, const vec3 wo, const Material mat,
                 const uint mode, const bool side, out vec3 dir,
                 out float pdf_w, out float cos_theta, const vec2 rands) {
    vec3 f;
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        dir = sample_cos_hemisphere(rands, n_s);
        f = diffuse_f(mat);
        pdf_w = diffuse_pdf(n_s, dir, cos_theta);
    } break;
    case BSDF_MIRROR: {
        dir = reflect(-wo, n_s);
        cos_theta = dot(n_s, dir);
        f = vec3(1.) / abs(cos_theta);
        pdf_w = 1.;
    } break;
    case BSDF_GLOSSY: {
        if (rands.x < .5) {
            const vec2 rands_new = vec2(2 * rands.x, rands.y);
            dir = sample_cos_hemisphere(rands_new, n_s);
        } else {
            const vec2 rands_new = vec2(2 * (rands.x - 0.5), rands.y);
            const vec3 f0 = mix(mat.albedo, vec3(0.04), mat.metalness);
            dir = sample_beckmann(rands_new, mat.roughness, n_s, wo);
            if (!same_hemisphere(wo, dir, n_s)) {
                pdf_w = 0.;
                f = vec3(0);
                return f;
            }
        }
        cos_theta = dot(n_s, dir);
        const vec3 h = normalize(wo + dir);
        float nh = max(0.00001f, min(1.0f, dot(n_s, h)));
        float hl = max(0.00001f, min(1.0f, dot(h, dir)));
        float nl = max(0.00001f, min(1.0f, dot(n_s, dir)));
        float nv = max(0.00001f, min(1.0f, dot(n_s, wo)));
        float beckmann_term = beckmann_d(mat.roughness, nh);
        f = glossy_f(mat, wo, dir, n_s, hl, nl, nv, beckmann_term);
        pdf_w = glossy_pdf(cos_theta, hl, nh, beckmann_term);
    } break;

    case BSDF_DISNEY: {

#if ENABLE_DISNEY
        const float diffuse_ratio = 0.5 * (1 - mat.metallic);
        if (rands.x < diffuse_ratio) {
            // Sample diffuse
            const vec2 rands_new = vec2(rands.x / diffuse_ratio, rands.y);
            dir = sample_cos_hemisphere(rands_new, n_s);
        } else {
            // Sample specular
            vec2 rands_new =
                vec2((rands.x - diffuse_ratio) / (1 - diffuse_ratio), rands.y);
            const float ratio_gtr2 = 1. / (1. + mat.clearcoat);
            if (rands_new.x < ratio_gtr2) {
                // Sample specular lobe
                rands_new.x /= ratio_gtr2;
                const float alpha = max(0.01f, sqr(mat.roughness));
                dir = sample_ggx_vndf(wo, alpha, alpha, rands_new.x,
                                      rands_new.y, n_s);
                if (!same_hemisphere(wo, dir, n_s)) {
                    pdf_w = 0.;
                    f = vec3(0);
                    return f;
                }
            } else {
                rands_new.x = (rands_new.x - ratio_gtr2) / (1 - ratio_gtr2);
                // Sample clearcoat
                const float alpha = mix(0.1, 0.001, mat.clearcoat_gloss);
                const float alpha2 = alpha * alpha;
                dir = sample_gtr1(rands_new, alpha2, n_s, wo);
                if (!same_hemisphere(wo, dir, n_s)) {
                    pdf_w = 0.;
                    f = vec3(0);
                    return f;
                }
            }
        }
        f = disney_f(mat, wo, dir, n_s);
        pdf_w = disney_pdf(n_s, mat, wo, dir);
        cos_theta = dot(n_s, dir);
#endif
    } break;
    case BSDF_GLASS: {
        const float ior = side ? 1. / mat.ior : mat.ior;

        // Refract
        const float cos_i = dot(n_s, wo);
        const float sin2_t = ior * ior * (1. - cos_i * cos_i);
        if (sin2_t >= 1.) {
            dir = reflect(-wo, n_s);
        } else {
            const float cos_t = sqrt(1 - sin2_t);
            dir = -ior * wo + (ior * cos_i - cos_t) * n_s;
        }
        cos_theta = dot(n_s, dir);
        f = vec3(1.) / abs(cos_theta);
        if (mode == 1) {
            f *= ior * ior;
        }
        pdf_w = 1.;

    } break;
    default: // Unknown
        break;
    }
    return f;
}

vec3 sample_bsdf(const vec3 n_s, const vec3 wo, const Material mat,
                 const uint mode, const bool side, out vec3 dir,
                 out float pdf_w, inout float cos_theta, inout uvec4 seed) {
    const vec2 rands = vec2(rand(seed), rand(seed));
    return sample_bsdf(n_s, wo, mat, mode, side, dir, pdf_w, cos_theta, rands);
}

vec3 eval_bsdf(const vec3 n_s, const vec3 wo, const Material mat,
               const uint mode, const bool side, const vec3 dir,
               out float pdf_w, float cos_theta) {
    if (!same_hemisphere(dir, wo, n_s)) {
        pdf_w = 0.;
        return vec3(0);
    }
    vec3 f;
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        f = diffuse_f(mat);
        pdf_w = diffuse_pdf(n_s, dir, wo);
    } break;
    case BSDF_MIRROR: {
        f = vec3(0);
        pdf_w = 0.;
    } break;
    case BSDF_GLASS: {
        f = vec3(0);
        pdf_w = 0.;
    } break;
    case BSDF_GLOSSY: {
        const vec3 h = normalize(wo + dir);
        float nh = max(0.00001f, min(1.0f, dot(n_s, h)));
        float hl = max(0.00001f, min(1.0f, dot(h, dir)));
        float nl = max(0.00001f, min(1.0f, dot(n_s, dir)));
        float nv = max(0.00001f, min(1.0f, dot(n_s, wo)));
        float beckmann_term = beckmann_d(mat.roughness, nh);
        f = glossy_f(mat, wo, dir, n_s, hl, nl, nv, beckmann_term);
        pdf_w = glossy_pdf(cos_theta, hl, nh, beckmann_term);
    } break;
    case BSDF_DISNEY: {
#if ENABLE_DISNEY
        f = disney_f(mat, wo, dir, n_s);
        pdf_w = disney_pdf(n_s, mat, wo, dir);
#else
        f = vec3(0);
        pdf_w = 0;
#endif
    } break;
    default: // Unknown
        break;
    }
    return f;
}
vec3 eval_bsdf(const vec3 n_s, const vec3 wo, const Material mat,
               const uint mode, const bool side, const vec3 dir,
               out float pdf_w, out float pdf_rev_w, in float cos_theta) {
    if (!same_hemisphere(dir, wo, n_s)) {
        pdf_w = 0;
        pdf_rev_w = 0;
        return vec3(0);
    } 
    vec3 f;
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        f = diffuse_f(mat);
        pdf_w = diffuse_pdf(n_s, dir, wo);
        pdf_rev_w = diffuse_pdf(n_s, wo, dir);
    } break;
    case BSDF_MIRROR: {
        f = vec3(0);
        pdf_w = 0.;
        pdf_rev_w = 0.;
    } break;
    case BSDF_GLASS: {
        f = vec3(0);
        pdf_w = 0.;
        pdf_rev_w = 0.;
    } break;
    case BSDF_GLOSSY: {
        const vec3 h = normalize(wo + dir);
        float nh = max(0.00001f, min(1.0f, dot(n_s, h)));
        float hl = max(0.00001f, min(1.0f, dot(h, dir)));
        float nl = max(0.00001f, min(1.0f, dot(n_s, dir)));
        float nv = max(0.00001f, min(1.0f, dot(n_s, wo)));
        float beckmann_term = beckmann_d(mat.roughness, nh);
        f = glossy_f(mat, wo, dir, n_s, hl, nl, nv, beckmann_term);
        pdf_w = glossy_pdf(cos_theta, hl, nh, beckmann_term);
        float cos_theta_wo = dot(wo, n_s);
        pdf_rev_w = glossy_pdf(cos_theta_wo, hl, nh, beckmann_term);
    } break;
    case BSDF_DISNEY: {
#if ENABLE_DISNEY
        f = disney_f(mat, wo, dir, n_s);
        pdf_w = disney_pdf(n_s, mat, wo, dir);
        pdf_rev_w = disney_pdf(n_s, mat, dir, wo);
#endif
    } break;
    default: // Unknown
        break;
    }
    return f;
}

vec3 eval_bsdf(const Material mat, const vec3 wo, const vec3 wi,
               const vec3 n_s) {
    if (!same_hemisphere(wi, wo, n_s)) {
        return vec3(0);
    } 
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        return diffuse_f(mat);
    } break;
    case BSDF_MIRROR:
    case BSDF_GLASS: {
        return vec3(0);
    } break;
    case BSDF_GLOSSY: {
        const vec3 h = normalize(wo + wi);
        float nh = max(0.00001f, min(1.0f, dot(n_s, h)));
        float hl = max(0.00001f, min(1.0f, dot(h, wi)));
        float nl = max(0.00001f, min(1.0f, dot(n_s, wi)));
        float nv = max(0.00001f, min(1.0f, dot(n_s, wo)));
        float beckmann_term = beckmann_d(mat.roughness, nh);
        return glossy_f(mat, wo, wi, n_s, hl, nl, nv, beckmann_term);
    } break;
    case BSDF_DISNEY: {
#if ENABLE_DISNEY
        return disney_f(mat, wo, wi, n_s);
#endif
    } break;
    default: {
        break;
    }
    }
    return vec3(0);
}

float bsdf_pdf(const Material mat, const vec3 n_s, const vec3 wo,
               const vec3 wi) {
    if (!same_hemisphere(wi, wo, n_s)) {
        return 0;
    } 
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        return diffuse_pdf(n_s, wi, wo);
    } break;
    case BSDF_GLOSSY: {
        const vec3 h = normalize(wo + wi);
        float nh = max(0.00001f, min(1.0f, dot(n_s, h)));
        float hl = max(0.00001f, min(1.0f, dot(h, wi)));
        float beckmann_term = beckmann_d(mat.roughness, nh);
        float cos_theta = dot(n_s, wi);
        return glossy_pdf(cos_theta, hl, nh, beckmann_term);
    } break;
    case BSDF_DISNEY: {
#if ENABLE_DISNEY
        return disney_pdf(n_s, mat, wo, wi);
#endif
    } break;
    }
    return 0;
}