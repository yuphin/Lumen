#ifndef COMMONS_DEVICE
#define COMMONS_DEVICE
#include "commons.h"
#include "utils.glsl"

layout(location = 0) rayPayloadEXT HitPayload payload;
layout(location = 1) rayPayloadEXT AnyHitPayload any_hit_payload;
layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 0, binding = 1, rgba32f) uniform image2D image;
layout(set = 0, binding = 2) readonly buffer InstanceInfo_ {
    PrimMeshInfo prim_info[];
};
layout(set = 0, binding = 3) uniform SceneUBOBuffer { SceneUBO ubo; };
layout(set = 0, binding = 4) buffer SceneDesc_ { SceneDesc scene_desc; };
layout(set = 0, binding = 5) uniform sampler2D scene_textures[];
layout(set = 0, binding = 6, scalar) readonly buffer Lights { Light lights[]; };

layout(buffer_reference, scalar) readonly buffer Vertices { vec3 v[]; };
layout(buffer_reference, scalar) readonly buffer Indices { uint i[]; };
layout(buffer_reference, scalar) readonly buffer Normals { vec3 n[]; };
layout(buffer_reference, scalar) readonly buffer TexCoords { vec2 t[]; };
layout(buffer_reference, scalar) readonly buffer Materials { Material m[]; };

Indices indices = Indices(scene_desc.index_addr);
Vertices vertices = Vertices(scene_desc.vertex_addr);
Normals normals = Normals(scene_desc.normal_addr);
Materials materials = Materials(scene_desc.material_addr);

vec4 sample_camera(in vec2 d) {
    vec4 target = ubo.inv_projection * vec4(d.x, d.y, 1, 1);
    return ubo.inv_view * vec4(normalize(target.xyz), 0); // direction
}

float correct_shading_normal(const vec3 geometry_nrm, const vec3 shading_nrm,
                             const vec3 wi, const vec3 wo, int mode) {
    if (mode == 0) {
        float num = abs(dot(wo, shading_nrm) * abs(dot(wi, geometry_nrm)));
        float denom = abs(dot(wo, geometry_nrm) * abs(dot(wi, shading_nrm)));
        if (denom == 0)
            return 0.;
        return num / denom;
    } else {
        return 1.;
    }
}

Material load_material(const uint material_idx, const vec2 uv) {
    Material m = materials.m[material_idx];
    if (m.texture_id > -1) {
        m.albedo *= texture(scene_textures[m.texture_id], uv).xyz;
    }
    return m;
}

float diffuse_pdf(const vec3 n, const vec3 l, out float cos_theta) {
    cos_theta = dot(n, l);
    return max(cos_theta / PI, 0);
}

float diffuse_pdf(const vec3 n, const vec3 l) { return max(dot(n, l) / PI, 0); }

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

vec3 glossy_f(const Material mat, const vec3 wo, const vec3 wi,
              const vec3 shading_nrm, float hl, float nl, float nv,
              float beckmann_term) {
    const vec3 h = normalize(wo + wi);
    const vec3 f_diffuse = (28. / (23 * PI)) * vec3(mat.albedo) *
                           (1 - mat.metalness) *
                           (1 - pow5(1 - 0.5 * dot(wi, shading_nrm))) *
                           (1 - pow5(1 - 0.5 * dot(wo, shading_nrm)));
    const vec3 f_specular = beckmann_term *
                            fresnel_schlick(vec3(mat.metalness), hl) /
                            (4 * hl * max(nl, nv));
    return f_specular + f_diffuse;
}

vec3 disney_f(const Material mat, const vec3 wo, const vec3 wi,
              const vec3 shading_nrm) {
    vec3 h = wo + wi;
    const float nl = abs(dot(shading_nrm, wi));
    const float nv = abs(dot(shading_nrm, wo));
    if(nl == 0 || nv == 0) {
        return vec3(0);
    }
    if(h.x == 0 && h.y == 0 && h.z == 0) {
        return vec3(0);
    }
    h = normalize(h);
    const float lh = dot(wi, h);
    const float nh = dot(shading_nrm, h);
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

vec3 sample_bsdf(const vec3 shading_nrm, const vec3 wo, const Material mat,
                 const uint mode, const bool side, out vec3 dir,
                 out float pdf_w, out float cos_theta, const vec2 rands) {
    vec3 f;
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        dir = sample_cos_hemisphere(rands, shading_nrm);
        f = diffuse_f(mat);
        pdf_w = diffuse_pdf(shading_nrm, dir, cos_theta);
    } break;
    case BSDF_MIRROR: {
        dir = reflect(-wo, shading_nrm);
        cos_theta = dot(shading_nrm, dir);
        f = vec3(1.) / abs(cos_theta);
        pdf_w = 1.;
    } break;
    case BSDF_GLOSSY: {
        if (rands.x < .5) {
            const vec2 rands_new = vec2(2 * rands.x, rands.y);
            dir = sample_cos_hemisphere(rands_new, shading_nrm);
        } else {
            const vec2 rands_new = vec2(2 * (rands.x - 0.5), rands.y);
            const vec3 f0 = mix(mat.albedo, vec3(0.04), mat.metalness);
            dir = sample_beckmann(rands_new, mat.roughness, shading_nrm, wo);
            if (!same_hemisphere(wo, dir, shading_nrm)) {
                pdf_w = 0.;
                f = vec3(0);
                return f;
            }
        }
        cos_theta = dot(shading_nrm, dir);
        const vec3 h = normalize(wo + dir);
        float nh = max(0.00001f, min(1.0f, dot(shading_nrm, h)));
        float hl = max(0.00001f, min(1.0f, dot(h, dir)));
        float nl = max(0.00001f, min(1.0f, dot(shading_nrm, dir)));
        float nv = max(0.00001f, min(1.0f, dot(shading_nrm, wo)));
        float beckmann_term = beckmann_d(mat.roughness, nh);
        f = glossy_f(mat, wo, dir, shading_nrm, hl, nl, nv, beckmann_term);
        pdf_w = glossy_pdf(cos_theta, hl, nh, beckmann_term);
    } break;

    case BSDF_DISNEY: {

#if ENABLE_DISNEY
        const float diffuse_ratio = 0.5 * (1 - mat.metallic);
        if (rands.x < diffuse_ratio) {
            // Sample diffuse
            const vec2 rands_new = vec2(rands.x / diffuse_ratio, rands.y);
            dir = sample_cos_hemisphere(rands_new, shading_nrm);
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
                                      rands_new.y, shading_nrm);
                if (!same_hemisphere(wo, dir, shading_nrm)) {
                    pdf_w = 0.;
                    f = vec3(0);
                    return f;
                }
            } else {
                rands_new.x = (rands_new.x - ratio_gtr2) / (1 - ratio_gtr2);
                // Sample clearcoat
                const float alpha = mix(0.1, 0.001, mat.clearcoat_gloss);
                const float alpha2 = alpha * alpha;
                dir = sample_gtr1(rands_new, alpha2, shading_nrm, wo);
                if (!same_hemisphere(wo, dir, shading_nrm)) {
                    pdf_w = 0.;
                    f = vec3(0);
                    return f;
                }
            }
        }
        f = disney_f(mat, wo, dir, shading_nrm);
        pdf_w = disney_pdf(shading_nrm, mat, wo, dir);
        cos_theta = dot(shading_nrm, dir);
#endif
    } break;
    case BSDF_GLASS: {
        const float ior = side ? 1. / mat.ior : mat.ior;

        // Refract
        const float cos_i = dot(shading_nrm, wo);
        const float sin2_t = ior * ior * (1. - cos_i * cos_i);
        if (sin2_t >= 1.) {
            dir = reflect(-wo, shading_nrm);
        } else {
            const float cos_t = sqrt(1 - sin2_t);
            dir = -ior * wo + (ior * cos_i - cos_t) * shading_nrm;
        }
        cos_theta = dot(shading_nrm, dir);
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

vec3 sample_bsdf(const vec3 shading_nrm, const vec3 wo, const Material mat,
                 const uint mode, const bool side, out vec3 dir,
                 out float pdf_w, inout float cos_theta, inout uvec4 seed) {
    const vec2 rands = vec2(rand(seed), rand(seed));
    return sample_bsdf(shading_nrm, wo, mat, mode, side, dir, pdf_w, cos_theta,
                       rands);
}

vec3 eval_bsdf(const vec3 shading_nrm, const vec3 wo, const Material mat,
               const uint mode, const bool side, const vec3 dir,
               out float pdf_w, float cos_theta) {
    vec3 f;
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        f = diffuse_f(mat);
        pdf_w = diffuse_pdf(shading_nrm, dir);
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
        if (!same_hemisphere(dir, wo, shading_nrm)) {
            f = vec3(0);
            pdf_w = 0.;
        } else {
            const vec3 h = normalize(wo + dir);
            float nh = max(0.00001f, min(1.0f, dot(shading_nrm, h)));
            float hl = max(0.00001f, min(1.0f, dot(h, dir)));
            float nl = max(0.00001f, min(1.0f, dot(shading_nrm, dir)));
            float nv = max(0.00001f, min(1.0f, dot(shading_nrm, wo)));
            float beckmann_term = beckmann_d(mat.roughness, nh);
            f = glossy_f(mat, wo, dir, shading_nrm, hl, nl, nv, beckmann_term);
            pdf_w = glossy_pdf(cos_theta, hl, nh, beckmann_term);
        }

    } break;
    case BSDF_DISNEY: {
#if ENABLE_DISNEY
        if (!same_hemisphere(dir, wo, shading_nrm)) {
            f = vec3(0);
            pdf_w = 0.;
        } else {
            f = disney_f(mat, wo, dir, shading_nrm);
            pdf_w = disney_pdf(shading_nrm, mat, wo, dir);
        }
#endif
    } break;
    default: // Unknown
        break;
    }
    return f;
}

vec3 eval_bsdf(const vec3 shading_nrm, const vec3 wo, const Material mat,
               const uint mode, const bool side, const vec3 dir,
               out float pdf_w, out float pdf_rev_w, in float cos_theta) {
    vec3 f;
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        f = diffuse_f(mat);
        pdf_w = diffuse_pdf(shading_nrm, dir);
        pdf_rev_w = diffuse_pdf(shading_nrm, wo);
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
        if (!same_hemisphere(dir, wo, shading_nrm)) {
            f = vec3(0);
            pdf_w = 0.;
        } else {
            const vec3 h = normalize(wo + dir);
            float nh = max(0.00001f, min(1.0f, dot(shading_nrm, h)));
            float hl = max(0.00001f, min(1.0f, dot(h, dir)));
            float nl = max(0.00001f, min(1.0f, dot(shading_nrm, dir)));
            float nv = max(0.00001f, min(1.0f, dot(shading_nrm, wo)));
            float beckmann_term = beckmann_d(mat.roughness, nh);
            f = glossy_f(mat, wo, dir, shading_nrm, hl, nl, nv, beckmann_term);
            pdf_w = glossy_pdf(cos_theta, hl, nh, beckmann_term);
            float cos_theta_wo = dot(wo, shading_nrm);
            pdf_rev_w = glossy_pdf(cos_theta_wo, hl, nh, beckmann_term);
        }
    } break;
    case BSDF_DISNEY: {
#if ENABLE_DISNEY
        f = disney_f(mat, wo, dir, shading_nrm);
        pdf_w = disney_pdf(shading_nrm, mat, wo, dir);
        pdf_rev_w = disney_pdf(shading_nrm, mat, dir, wo);
#endif
    } break;
    default: // Unknown
        break;
    }
    return f;
}

vec3 eval_bsdf(const Material mat, const vec3 wo, const vec3 wi,
               const vec3 shading_nrm) {
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        return diffuse_f(mat);
    } break;
    case BSDF_MIRROR:
    case BSDF_GLASS: {
        return vec3(0);
    } break;
    case BSDF_GLOSSY: {
        if (!same_hemisphere(wi, wo, shading_nrm)) {
            return vec3(0);
        }
        const vec3 h = normalize(wo + wi);
        float nh = max(0.00001f, min(1.0f, dot(shading_nrm, h)));
        float hl = max(0.00001f, min(1.0f, dot(h, wi)));
        float nl = max(0.00001f, min(1.0f, dot(shading_nrm, wi)));
        float nv = max(0.00001f, min(1.0f, dot(shading_nrm, wo)));
        float beckmann_term = beckmann_d(mat.roughness, nh);
        return glossy_f(mat, wo, wi, shading_nrm, hl, nl, nv, beckmann_term);
    } break;
    case BSDF_DISNEY: {
#if ENABLE_DISNEY
        return disney_f(mat, wo, wi, shading_nrm);
#endif
    } break;
    default: {
        break;
    }
    }
    return vec3(0);
}

float bsdf_pdf(const Material mat, const vec3 shading_nrm, const vec3 wo,
               const vec3 wi) {
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        return diffuse_pdf(shading_nrm, wi);
    } break;
    case BSDF_GLOSSY: {
        if (!same_hemisphere(wo, wi, shading_nrm)) {
            return 0;
        }
        const vec3 h = normalize(wo + wi);
        float nh = max(0.00001f, min(1.0f, dot(shading_nrm, h)));
        float hl = max(0.00001f, min(1.0f, dot(h, wi)));
        float beckmann_term = beckmann_d(mat.roughness, nh);
        float cos_theta = dot(shading_nrm, wi);
        return glossy_pdf(cos_theta, hl, nh, beckmann_term);
    } break;
    case BSDF_DISNEY: {
#if ENABLE_DISNEY
        return disney_pdf(shading_nrm, mat, wo, wi);
#endif
    } break;
    }
    return 0;
}

float uniform_cone_pdf(float cos_max) { return 1. / (PI2 * (1 - cos_max)); }

float light_pdf(const Light light, const vec3 shading_nrm, const vec3 wi) {
    const float cos_width = cos(30 * PI / 180);
    switch (light.light_flags) {
    case LIGHT_AREA: {
        return max(dot(shading_nrm, wi) / PI, 0);
    } break;
    case LIGHT_SPOT: {
        return uniform_cone_pdf(cos_width);
    } break;
    case LIGHT_DIRECTIONAL: {
        return 0;
    } break;
    }
}

float light_pdf(uint light_flags, const vec3 shading_nrm, const vec3 wi) {
    const float cos_width = cos(30 * PI / 180);
    switch (light_flags) {
    case LIGHT_AREA: {
        return max(dot(shading_nrm, wi) / PI, 0);
    }
    case LIGHT_SPOT: {
        return uniform_cone_pdf(cos_width);
    }
    case LIGHT_DIRECTIONAL: {
        return 0;
    }
    }
}

TriangleRecord sample_triangle(PrimMeshInfo pinfo, vec2 rands,
                               uint triangle_idx, in mat4 world_matrix,
                               out float o_u, out float o_v) {
    TriangleRecord result;
    uint index_offset = pinfo.index_offset + 3 * triangle_idx;
    uint vertex_offset = pinfo.vertex_offset;
    ivec3 ind = ivec3(indices.i[index_offset + 0], indices.i[index_offset + 1],
                      indices.i[index_offset + 2]);
    ind += ivec3(vertex_offset);
    const vec4 v0 = vec4(vertices.v[ind.x], 1.0);
    const vec4 v1 = vec4(vertices.v[ind.y], 1.0);
    const vec4 v2 = vec4(vertices.v[ind.z], 1.0);

    const vec4 n0 = vec4(normals.n[ind.x], 1.0);
    const vec4 n1 = vec4(normals.n[ind.y], 1.0);
    const vec4 n2 = vec4(normals.n[ind.z], 1.0);
    //    mat4x3 matrix = mat4x3(vec3(world_matrix[0]), vec3(world_matrix[1]),
    //                           vec3(world_matrix[2]), vec3(world_matrix[3]));
    mat4x4 inv_tr_mat = transpose(inverse(world_matrix));
    //    mat4x3 nrm_mat = mat4x3(vec3(inv_tr_mat[0]), vec3(inv_tr_mat[1]),
    //                            vec3(inv_tr_mat[2]), vec3(inv_tr_mat[3]));
    float u = 1 - sqrt(rands.x);
    float v = rands.y * sqrt(rands.x);
    const vec3 barycentrics = vec3(1.0 - u - v, u, v);

    const vec4 etmp0 = world_matrix * (v1 - v0);
    const vec4 etmp1 = world_matrix * (v2 - v0);
    const vec4 pos =
        v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
    const vec4 nrm = normalize(n0 * barycentrics.x + n1 * barycentrics.y +
                               n2 * barycentrics.z);
    const vec4 world_pos = world_matrix * pos;

    result.triangle_normal = normalize(vec3(inv_tr_mat * nrm));
    result.triangle_pdf = 2. / length((cross(vec3(etmp0), vec3(etmp1))));
    result.pos = vec3(world_pos);
    o_u = u;
    o_v = v;
    return result;
}

TriangleRecord sample_triangle(PrimMeshInfo pinfo, vec2 rands,
                               uint triangle_idx, in mat4 world_matrix) {
    float u, v;
    return sample_triangle(pinfo, rands, triangle_idx, world_matrix, u, v);
}

vec3 eval_albedo(const Material m) {
    vec3 albedo = m.albedo;
    if (m.texture_id > -1) {
        albedo *= texture(scene_textures[m.texture_id], payload.uv).xyz;
    }
    return albedo;
}

TriangleRecord sample_area_light(const vec4 rands, const int num_lights,
                                 const Light light, out uint triangle_idx,
                                 out uint material_idx) {
    PrimMeshInfo pinfo = prim_info[light.prim_mesh_idx];
    material_idx = pinfo.material_index;
    triangle_idx = uint(rands.y * light.num_triangles);
    return sample_triangle(pinfo, rands.zw, triangle_idx, light.world_matrix);
}

TriangleRecord sample_area_light(const vec4 rands, const int num_lights,
                                 const Light light, out uint triangle_idx,
                                 out uint material_idx, out float u,
                                 out float v) {
    PrimMeshInfo pinfo = prim_info[light.prim_mesh_idx];
    material_idx = pinfo.material_index;
    // triangle_idx = 6;
    triangle_idx = uint(rands.y * light.num_triangles);
    return sample_triangle(pinfo, rands.zw, triangle_idx, light.world_matrix, u,
                           v);
}

TriangleRecord sample_area_light_with_idx(const vec4 rands,
                                          const int num_lights,
                                          const Light light,
                                          const uint triangle_idx,
                                          out uint material_idx) {
    PrimMeshInfo pinfo = prim_info[light.prim_mesh_idx];
    material_idx = pinfo.material_index;
    return sample_triangle(pinfo, rands.zw, triangle_idx, light.world_matrix);
}

vec3 sample_light(const vec4 rands_pos, const vec3 p, const int num_lights,
                  out uint light_idx, out uint triangle_idx,
                  out uint material_idx, out Light light,
                  out TriangleRecord record, out Material light_mat,
                  inout uvec4 seed) {
    light_idx = uint(rands_pos.x * num_lights);
    light = lights[light_idx];
    vec3 L = vec3(0);
    if (light.light_flags == LIGHT_AREA) {
        record = sample_area_light(rands_pos, num_lights, light, triangle_idx,
                                   material_idx);
        vec2 uv_unused;
        light_mat = load_material(material_idx, uv_unused);
        L = light_mat.emissive_factor;
    } else if (light.light_flags == LIGHT_SPOT) {
        vec3 dir = normalize(p - light.pos);
        const vec3 light_dir = normalize(light.to - light.pos);
        const float cos_theta = dot(dir, light_dir);
        const float cos_width = cos(PI / 6);
        const float cos_faloff = cos(25 * PI / 180);
        float faloff;
        if (cos_theta < cos_width) {
            faloff = 0;
        } else if (cos_theta >= cos_faloff) {
            faloff = 1;
        } else {
            float d = (cos_theta - cos_width) / (cos_faloff - cos_width);
            faloff = (d * d) * (d * d);
        }
        record.pos = light.pos;
        record.triangle_pdf = 1.;
        record.triangle_normal = dir;
        L = light.L * faloff;
    } else if (light.light_flags == LIGHT_DIRECTIONAL) {
        vec3 dir = -normalize(light.to - light.pos);
        record.pos = p + dir * (2 * light.world_radius);
        record.triangle_pdf = 1.;
        record.triangle_normal = -dir;
        L = light.L;
    }
    return L;
}

vec3 sample_light_with_idx(const vec4 rands_pos, const vec3 p,
                           const int num_lights, const uint light_idx,
                           const uint triangle_idx, out uint material_idx,
                           out Light light, out TriangleRecord record,
                           out Material light_mat) {
    light = lights[light_idx];
    vec3 L = vec3(0);
    if (light.light_flags == LIGHT_AREA) {
        record = sample_area_light_with_idx(rands_pos, num_lights, light,
                                            triangle_idx, material_idx);
        vec2 uv_unused;
        light_mat = load_material(material_idx, uv_unused);
        L = light_mat.emissive_factor;
    } else if (light.light_flags == LIGHT_SPOT) {
        vec3 dir = normalize(p - light.pos);
        const vec3 light_dir = normalize(light.to - light.pos);
        const float cos_theta = dot(dir, light_dir);
        const float cos_width = cos(PI / 6);
        const float cos_faloff = cos(25 * PI / 180);
        float faloff;
        if (cos_theta < cos_width) {
            faloff = 0;
        } else if (cos_theta >= cos_faloff) {
            faloff = 1;
        } else {
            float d = (cos_theta - cos_width) / (cos_faloff - cos_width);
            faloff = (d * d) * (d * d);
        }
        record.pos = light.pos;
        record.triangle_pdf = 1.;
        record.triangle_normal = dir;
        L = light.L * faloff;
    } else if (light.light_flags == LIGHT_DIRECTIONAL) {
        vec3 dir = -normalize(light.to - light.pos);
        record.pos = p + dir * (2 * light.world_radius);
        record.triangle_pdf = 1.;
        record.triangle_normal = -dir;
        L = light.L;
    }
    return L;
}

vec3 sample_light(const vec3 p, const int num_lights, out uint light_idx,
                  out uint triangle_idx, out uint material_idx, out Light light,
                  out TriangleRecord record, out Material light_mat,
                  inout uvec4 seed) {
    const vec4 rands = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    return sample_light(rands, p, num_lights, light_idx, triangle_idx,
                        material_idx, light, record, light_mat, seed);
}

vec3 uniform_sample_cone(vec2 uv, float cos_max) {
    const float cos_theta = (1. - uv.x) + uv.x * cos_max;
    const float sin_theta = sqrt(1 - cos_theta * cos_theta);
    const float phi = uv.y * PI2;
    return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

vec3 sample_light_Le(const vec4 rands_pos, const vec2 rands_dir,
                     const int num_lights, const int total_light,
                     out uint light_idx, out uint triangle_idx,
                     out uint material_idx, out Light light,
                     out TriangleRecord record, out Material light_mat,
                     out float pdf_pos, inout uvec4 seed, out vec3 wi,
                     out float pdf_dir, out float phi, out float u,
                     out float v) {
    light_idx = uint(rands_pos.x * num_lights);
    light = lights[light_idx];
    vec3 L = vec3(0);
    if (light.light_flags == LIGHT_AREA) {
        record = sample_area_light(rands_pos, num_lights, light, triangle_idx,
                                   material_idx, u, v);
        vec2 uv_unused;
        light_mat = load_material(material_idx, uv_unused);
        pdf_pos = record.triangle_pdf / total_light;
        L = light_mat.emissive_factor;
        wi = sample_cos_hemisphere(rands_dir, record.triangle_normal, phi);
        pdf_dir = (dot(wi, record.triangle_normal)) / PI;
    } else if (light.light_flags == LIGHT_SPOT) {
        const float cos_width = cos(30 * PI / 180);
        const float cos_faloff = cos(25 * PI / 180);
        const vec3 light_dir = normalize(light.to - light.pos);
        vec4 local_quat = to_local_quat(light_dir);
        wi = rot_quat(invert_quat(local_quat),
                      uniform_sample_cone(rands_dir, cos_width));
        const float cos_theta = dot(wi, light_dir);
        float faloff;
        if (cos_theta < cos_width) {
            faloff = 0;
        } else if (cos_theta >= cos_faloff) {
            faloff = 1;
        } else {
            float d = (cos_theta - cos_width) / (cos_faloff - cos_width);
            faloff = (d * d) * (d * d);
        }
        record.pos = light.pos;
        record.triangle_pdf = 1.;
        record.triangle_normal = wi;
        L = light.L * faloff;
        pdf_pos = 1.;
        pdf_dir = uniform_cone_pdf(cos_width);
        u = 0;
        v = 0;
    } else if (light.light_flags == LIGHT_DIRECTIONAL) {
        vec3 dir = -normalize(light.to - light.pos);
        vec3 v1, v2;
        make_coord_system(dir, v1, v2);
        vec2 uv = concentric_sample_disk(rands_dir);
        vec3 pos = light.world_center + light.world_radius * (u * v1 + v * v2);
        record.pos = pos + dir * light.world_radius;
        record.triangle_normal = -dir;
        L = light.L;
        pdf_pos = 1. / (PI * light.world_radius * light.world_radius);
        record.triangle_pdf = pdf_pos;
        pdf_dir = 1.;
        u = 0;
        v = 0;
    }
    return L;
}

vec3 sample_light_Le(const int num_lights, const int total_light,
                     out uint light_idx, out uint triangle_idx,
                     out uint material_idx, out Light light,
                     out TriangleRecord record, out Material light_mat,
                     inout uvec4 seed, out vec3 wi, out float u, out float v) {
    const vec4 rands_pos = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    const vec2 rands_dir = vec2(rand(seed), rand(seed));
    float phi, pdf_pos, pdf_dir;
    return sample_light_Le(rands_pos, rands_dir, num_lights, total_light,
                           light_idx, triangle_idx, material_idx, light, record,
                           light_mat, pdf_pos, seed, wi, pdf_dir, phi, u, v);
}

vec3 sample_light_Le(const vec4 rands_pos, const vec2 rands_dir,
                     const int num_lights, const int total_light,
                     out uint light_idx, out uint triangle_idx,
                     out uint material_idx, out Light light,
                     out TriangleRecord record, out Material light_mat,
                     out float pdf_pos, inout uvec4 seed, out vec3 wi,
                     out float pdf_dir) {
    float phi, u, v;
    return sample_light_Le(rands_pos, rands_dir, num_lights, total_light,
                           light_idx, triangle_idx, material_idx, light, record,
                           light_mat, pdf_pos, seed, wi, pdf_dir, phi, u, v);
}

vec3 sample_light_Le(const int num_lights, const int total_light,
                     out uint light_idx, out uint triangle_idx,
                     out uint material_idx, out Light light,
                     out TriangleRecord record, out Material light_mat,
                     out float pdf_pos, inout uvec4 seed, out vec3 wi,
                     out float pdf_dir) {
    const vec4 rands_pos = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    const vec2 rands_dir = vec2(rand(seed), rand(seed));
    float phi, u, v;
    return sample_light_Le(rands_pos, rands_dir, num_lights, total_light,
                           light_idx, triangle_idx, material_idx, light, record,
                           light_mat, pdf_pos, seed, wi, pdf_dir, phi, u, v);
}

vec3 sample_light_Le(const int num_lights, const int total_light,
                     out uint light_idx, out uint triangle_idx,
                     out uint material_idx, out Light light,
                     out TriangleRecord record, out Material light_mat,
                     out float pdf_pos, inout uvec4 seed, out vec3 wi,
                     out float pdf_dir, out float phi) {
    const vec4 rands_pos = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    const vec2 rands_dir = vec2(rand(seed), rand(seed));
    float u, v;
    return sample_light_Le(rands_pos, rands_dir, num_lights, total_light,
                           light_idx, triangle_idx, material_idx, light, record,
                           light_mat, pdf_pos, seed, wi, pdf_dir, phi, u, v);
}

vec3 sample_light_Le(const int num_lights, const int total_light,
                     out uint light_idx, out uint triangle_idx,
                     out uint material_idx, out Light light,
                     out TriangleRecord record, out Material light_mat,
                     out float pdf_pos, inout uvec4 seed, out vec3 wi,
                     out float pdf_dir, out float phi, out float u,
                     out float v) {
    const vec4 rands_pos = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    const vec2 rands_dir = vec2(rand(seed), rand(seed));
    return sample_light_Le(rands_pos, rands_dir, num_lights, total_light,
                           light_idx, triangle_idx, material_idx, light, record,
                           light_mat, pdf_pos, seed, wi, pdf_dir, phi, u, v);
}

vec3 sample_light_w(const int num_lights, const int total_light,
                    out uint light_idx, out uint triangle_idx,
                    out uint material_idx, out Light light,
                    out TriangleRecord record, out Material light_mat,
                    inout uvec4 seed, out vec3 wi, out float u, out float v) {
    const vec4 rands = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    light_idx = uint(rands.x * num_lights);
    light = lights[light_idx];
    vec3 L = vec3(0);
    if (light.light_flags == LIGHT_AREA) {
        record = sample_area_light(rands, num_lights, light, triangle_idx,
                                   material_idx, u, v);
        vec2 uv_unused;
        light_mat = load_material(material_idx, uv_unused);
        L = light_mat.emissive_factor;
        const vec2 rands_dir = vec2(rand(seed), rand(seed));
        wi = sample_cos_hemisphere(vec2(rand(seed), rand(seed)),
                                   record.triangle_normal);
    } else if (light.light_flags == LIGHT_SPOT) {
        const float cos_width = cos(30 * PI / 180);
        const float cos_faloff = cos(25 * PI / 180);
        const vec3 light_dir = normalize(light.to - light.pos);
        vec4 local_quat = to_local_quat(light_dir);
        const vec2 rands = vec2(rand(seed), rand(seed));
        wi = rot_quat(invert_quat(local_quat),
                      uniform_sample_cone(rands, cos_width));
        const float cos_theta = dot(wi, light_dir);
        float faloff;
        if (cos_theta < cos_width) {
            faloff = 0;
        } else if (cos_theta >= cos_faloff) {
            faloff = 1;
        } else {
            float d = (cos_theta - cos_width) / (cos_faloff - cos_width);
            faloff = (d * d) * (d * d);
        }
        record.pos = light.pos;
        record.triangle_pdf = 1.;
        record.triangle_normal = wi;
        L = light.L * faloff;
        u = 0;
        v = 0;
    } else if (light.light_flags == LIGHT_DIRECTIONAL) {
        vec3 dir = -normalize(light.to - light.pos);
        vec3 v1, v2;
        make_coord_system(dir, v1, v2);
        const vec2 rands = vec2(rand(seed), rand(seed));
        vec2 uv = concentric_sample_disk(rands);
        vec3 pos = light.world_center + light.world_radius * (u * v1 + v * v2);
        record.pos = pos + dir * light.world_radius;
        record.triangle_normal = -dir;
        L = light.L;
        float pdf_pos = 1. / (PI * light.world_radius * light.world_radius);
        record.triangle_pdf = pdf_pos;
        u = 0;
        v = 0;
    }

    return L;
}

#endif