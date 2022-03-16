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

MaterialProps load_material(const uint material_idx, const vec2 uv) {
    MaterialProps res;
    const Material mat = materials.m[material_idx];
    res.emissive_factor = mat.emissive_factor;
    res.albedo = vec3(mat.base_color_factor);
    res.bsdf_type = mat.bsdf_type;
    res.bsdf_props = 0;
    switch (res.bsdf_type) {
    case BSDF_DIFFUSE:
        res.bsdf_props |= BSDF_OPAQUE;
        res.bsdf_props |= BSDF_LAMBERTIAN;
        break;
    case BSDF_GLASS:
        res.bsdf_props |= BSDF_SPECULAR;
        res.bsdf_props |= BSDF_TRANSMISSIVE;
        break;
    case BSDF_MIRROR:
        res.bsdf_props |= BSDF_SPECULAR;
        res.bsdf_props |= BSDF_REFLECTIVE;
        break;
    case BSDF_GLOSSY:
        res.bsdf_props |= BSDF_OPAQUE;
        res.bsdf_props |= BSDF_LAMBERTIAN;
        res.bsdf_props |= BSDF_REFLECTIVE;
        break;
    default:
        break;
    }
    res.ior = mat.ior;
    res.metalness = mat.metalness;
    res.roughness = mat.roughness;
    return res;
}

vec3 sample_bsdf(const vec3 shading_nrm, const vec3 wo, const MaterialProps mat,
                 const uint mode, const bool side, out vec3 dir,
                 out float pdf_w, inout float cos_theta, const vec2 rands) {
    vec3 f;
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        f = mat.albedo / PI;
        dir = sample_cos_hemisphere(rands, shading_nrm);
        cos_theta = dot(shading_nrm, dir);
        pdf_w = max(cos_theta / PI, 0);
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
            cos_theta = dot(shading_nrm, dir);
        } else {
            const vec2 rands_new = vec2(2 * (rands.x - 0.5), rands.y);
            const vec3 f0 = mix(mat.albedo, vec3(0.04), mat.metalness);
            dir = sample_beckmann(rands_new, mat.roughness, shading_nrm, wo);
            cos_theta = dot(shading_nrm, dir);
            if (!same_hemisphere(wo, dir, shading_nrm)) {
                pdf_w = 0.;
                f = vec3(0);
                return f;
            }
        }
#define pow5(x) (x * x) * (x * x) * x
        const vec3 f_diffuse = (28. / (23 * PI)) * vec3(mat.albedo) *
                               (1 - mat.metalness) *
                               (1 - pow5(1 - 0.5 * dot(dir, shading_nrm))) *
                               (1 - pow5(1 - 0.5 * dot(wo, shading_nrm)));
        const vec3 h = normalize(wo + dir);
        float nh = max(0.00001f, min(1.0f, dot(shading_nrm, h)));
        float hl = max(0.00001f, min(1.0f, dot(h, dir)));
        float nl = max(0.00001f, min(1.0f, dot(shading_nrm, dir)));
        float nv = max(0.00001f, min(1.0f, dot(shading_nrm, wo)));
        float beckmann_term = beckmann_d(mat.roughness, nh);
        const vec3 f_specular = beckmann_term *
                                fresnel_schlick(vec3(mat.metalness), hl) /
                                (4 * hl * max(nl, nv));
        f = f_specular + f_diffuse;
        pdf_w = 0.5 * (max(cos_theta / PI, 0) + beckmann_term * nh / (4 * hl));

#undef pow5
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

vec3 sample_bsdf(const vec3 shading_nrm, const vec3 wo, const MaterialProps mat,
                 const uint mode, const bool side, out vec3 dir,
                 out float pdf_w, inout float cos_theta, inout uvec4 seed) {
    const vec2 rands = vec2(rand(seed), rand(seed));
    return sample_bsdf(shading_nrm, wo, mat, mode, side, dir, pdf_w, cos_theta,
                       rands);
}

vec3 eval_bsdf(const vec3 shading_nrm, const vec3 wo, const MaterialProps mat,
               const uint mode, const bool side, const vec3 dir,
               out float pdf_w, in float cos_theta) {
    vec3 f;
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        f = mat.albedo / PI;
        pdf_w = max(cos_theta / PI, 0);
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
#define pow5(x) (x * x) * (x * x) * x
            const vec3 f_diffuse = (28. / (23 * PI)) * vec3(mat.albedo) *
                                   (1 - mat.metalness) *
                                   (1 - pow5(1 - 0.5 * dot(dir, shading_nrm))) *
                                   (1 - pow5(1 - 0.5 * dot(wo, shading_nrm)));
            const vec3 h = normalize(wo + dir);
            float nh = max(0.00001f, min(1.0f, dot(shading_nrm, h)));
            float hl = max(0.00001f, min(1.0f, dot(h, dir)));
            float nl = max(0.00001f, min(1.0f, dot(shading_nrm, dir)));
            float nv = max(0.00001f, min(1.0f, dot(shading_nrm, wo)));
            float beckmann_term = beckmann_d(mat.roughness, nh);
            const vec3 f_specular = beckmann_term *
                                    fresnel_schlick(mat.metalness, hl) /
                                    (4 * hl * max(nl, nv));
            f = f_specular + f_diffuse;

            pdf_w =
                0.5 * (max(cos_theta / PI, 0) + beckmann_term * nh / (4 * hl));

#undef pow5
        }

    } break;
    default: // Unknown
        break;
    }
    return f;
}

vec3 eval_bsdf(const vec3 shading_nrm, const vec3 wo, const MaterialProps mat,
               const uint mode, const bool side, const vec3 dir,
               out float pdf_w, out float pdf_rev_w, in float cos_theta) {
    vec3 f;
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        f = mat.albedo / PI;
        pdf_w = max(cos_theta / PI, 0);
        pdf_rev_w = max(dot(shading_nrm, wo) / PI, 0);
    } break;
    case BSDF_MIRROR: {
        f = vec3(0);
        pdf_w = 0.;
        pdf_rev_w = 0.;
    } break;
    case BSDF_GLASS: {
        const float ior = side ? 1. / mat.ior : mat.ior;
        f = vec3(0);
        pdf_w = 0.;
        pdf_rev_w = 0.;
    } break;
    case BSDF_GLOSSY: {
        if (!same_hemisphere(dir, wo, shading_nrm)) {
            f = vec3(0);
            pdf_w = 0.;
        } else {
#define pow5(x) (x * x) * (x * x) * x
            const vec3 f_diffuse = (28. / (23 * PI)) * vec3(mat.albedo) *
                                   (1 - mat.metalness) *
                                   (1 - pow5(1 - 0.5 * dot(dir, shading_nrm))) *
                                   (1 - pow5(1 - 0.5 * dot(wo, shading_nrm)));
            const vec3 h = normalize(wo + dir);
            float nh = max(0.00001f, min(1.0f, dot(shading_nrm, h)));
            float hl = max(0.00001f, min(1.0f, dot(h, dir)));
            float nl = max(0.00001f, min(1.0f, dot(shading_nrm, dir)));
            float nv = max(0.00001f, min(1.0f, dot(shading_nrm, wo)));
            float beckmann_term = beckmann_d(mat.roughness, nh);
            const vec3 f_specular = beckmann_term *
                                    fresnel_schlick(mat.metalness, hl) /
                                    (4 * hl * max(nl, nv));
            f = f_specular + f_diffuse;

            pdf_w =
                0.5 * (max(cos_theta / PI, 0) + beckmann_term * nh / (4 * hl));
            pdf_rev_w = 0.5 * (max(dot(shading_nrm, wo) / PI, 0) +
                               beckmann_term * nh / (4 * hl));

#undef pow5
        }
    } break;
    default: // Unknown
        break;
    }

    return f;
}

vec3 eval_bsdf(const MaterialProps mat, const vec3 wo, const vec3 wi,
               const vec3 shading_nrm) {
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        return mat.albedo / PI;
    } break;
    case BSDF_MIRROR:
    case BSDF_GLASS: {
        return vec3(0);
    } break;
    case BSDF_GLOSSY: {
        if (!same_hemisphere(wi, wo, shading_nrm)) {
            return vec3(0);
        }
#define pow5(x) (x * x) * (x * x) * x
        const vec3 f_diffuse = (28. / (23 * PI)) * vec3(mat.albedo) *
                               (1 - mat.metalness) *
                               (1 - pow5(1 - 0.5 * dot(wi, shading_nrm))) *
                               (1 - pow5(1 - 0.5 * dot(wo, shading_nrm)));
        const vec3 h = normalize(wo + wi);
        float nh = max(0.00001f, min(1.0f, dot(shading_nrm, h)));
        float hl = max(0.00001f, min(1.0f, dot(h, wi)));
        float nl = max(0.00001f, min(1.0f, dot(shading_nrm, wi)));
        float nv = max(0.00001f, min(1.0f, dot(shading_nrm, wo)));
        float beckmann_term = beckmann_d(mat.roughness, nh);
        const vec3 f_specular = beckmann_term *
                                fresnel_schlick(mat.metalness, hl) /
                                (4 * hl * max(nl, nv));
        return f_specular + f_diffuse;
#undef pow5
    } break;
    default: {
        break;
    }
    }
    if ((mat.bsdf_props & BSDF_LAMBERTIAN) == BSDF_LAMBERTIAN) {
        return mat.albedo / PI;
    } else if ((mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR) {
        return vec3(0);
    }
    return vec3(0);
}

// vec3 eval_bsdf_selective(const vec3 shading_nrm, const vec3 wo,
//                          const MaterialProps mat, const uint mode,
//                          const bool side, const uint bsdf_props, const vec3
//                          dir, out float pdf_w, in float cos_theta) {
//     if ((bsdf_props & BSDF_ALL) != 0) {
//         return eval_bsdf(shading_nrm, wo, mat, mode, side, dir, pdf_w,
//                          cos_theta);
//     } else if (bsdf_props == (BSDF_ALL & ~BSDF_SPECULAR)) {
//         if (mat.bsdf_type == BSDF_DIFFUSE) {
//             vec3 f = mat.albedo / PI;
//             pdf_w = max(cos_theta / PI, 0);
//             return f;
//         }
//     }
//     return vec3(0);
// }

// vec3 sample_bsdf_selective(const vec3 shading_nrm, const vec3 wo,
//                            const MaterialProps mat, const uint mode,
//                            const bool side, const uint bsdf_props, out vec3
//                            dir, out float pdf_w, out float cos_theta, inout
//                            uvec4 seed) {
//     if ((bsdf_props & BSDF_ALL) != 0) {
//         return sample_bsdf(shading_nrm, wo, mat, mode, side, dir, pdf_w,
//                            cos_theta, seed);
//     } else if (bsdf_props == (BSDF_ALL & ~BSDF_SPECULAR)) {
//         if (mat.bsdf_type == BSDF_DIFFUSE) {
//             vec3 f = mat.albedo / PI;
//             dir = sample_cos_hemisphere(vec2(rand(seed), rand(seed)),
//                                         shading_nrm);
//             cos_theta = dot(shading_nrm, dir);
//             pdf_w = max(cos_theta / PI, 0);
//             return f;
//         }
//     }
//     return vec3(0);
// }

float bsdf_pdf(const MaterialProps mat, const vec3 shading_nrm, const vec3 wo,
               const vec3 wi) {
    switch (mat.bsdf_type) {
    case BSDF_DIFFUSE: {
        return max(dot(shading_nrm, wi) / PI, 0);
    };
    case BSDF_GLOSSY: {
        if (!same_hemisphere(wo, wi, shading_nrm)) {
            return 0;
        }
        const vec3 h = normalize(wo + wi);
        float nh = max(0.00001f, min(1.0f, dot(shading_nrm, h)));
        float hl = max(0.00001f, min(1.0f, dot(h, wi)));
        float beckmann_term = beckmann_d(mat.roughness, nh);
        float pdf = 0.5 * (max(dot(shading_nrm, wi) / PI, 0) +
                           beckmann_term * nh / (4 * hl));
        return pdf;
    };
    }
    return 0;
    // if ((mat.bsdf_props & BSDF_LAMBERTIAN) == BSDF_LAMBERTIAN) {
    //     return max(dot(shading_nrm, wi) / PI, 0);
    // } else if ((mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR) {
    //     return 0;
    // }
    // return 0;
}

float uniform_cone_pdf(float cos_max) { return 1. / (PI2 * (1 - cos_max)); }

float light_pdf(const Light light, const vec3 shading_nrm, const vec3 wi) {
    const float cos_width = cos(30 * PI / 180);
    switch (light.light_flags) {
    case LIGHT_AREA: {
        return max(dot(shading_nrm, wi) / PI, 0);
    }
    case LIGHT_SPOT: {
        return uniform_cone_pdf(cos_width);
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
    vec3 albedo = m.base_color_factor.xyz;
    if (m.texture_id > -1) {
        albedo *= texture(scene_textures[m.texture_id], payload.uv).xyz;
    }
    return albedo;
}

// TriangleRecord sample_area_light_with_idx(const vec4 rands,
//                                           const uint light_mesh_idx,
//                                           const uint triangle_idx,
//                                           out uint material_idx) {
//     const Light light = lights[light_mesh_idx];
//     PrimMeshInfo pinfo = prim_info[light.prim_mesh_idx];
//     material_idx = pinfo.material_index;
//     return sample_triangle(pinfo, rands.zw, triangle_idx,
//     light.world_matrix);
// }

// TriangleRecord sample_area_light_with_idx(const vec2 rands,
//                                           const uint light_mesh_idx,
//                                           const uint triangle_idx,
//                                           out uint material_idx,
//                                           out vec3 min_pos, out vec3 max_pos,
//                                           out float u, out float v) {
//     const Light light = lights[light_mesh_idx];
//     PrimMeshInfo pinfo = prim_info[light.prim_mesh_idx];
//     material_idx = pinfo.material_index;
//     min_pos = pinfo.min_pos.xyz;
//     max_pos = pinfo.max_pos.xyz;
//     return sample_triangle(pinfo, rands.xy, triangle_idx, light.world_matrix,
//     u,
//                            v);
// }

// TriangleRecord sample_area_light_with_idx(const vec2 rands,
//                                           const uint light_mesh_idx,
//                                           const uint triangle_idx,
//                                           out uint material_idx, out float u,
//                                           out float v) {
//     const Light light = lights[light_mesh_idx];
//     PrimMeshInfo pinfo = prim_info[light.prim_mesh_idx];
//     material_idx = pinfo.material_index;
//     return sample_triangle(pinfo, rands.xy, triangle_idx, light.world_matrix,
//     u,
//                            v);
// }

// TriangleRecord sample_area_light_with_idx(const vec2 rands,
//                                           const uint light_mesh_idx,
//                                           const uint triangle_idx,
//                                           out uint material_idx) {
//     const Light light = lights[light_mesh_idx];
//     PrimMeshInfo pinfo = prim_info[light.prim_mesh_idx];
//     material_idx = pinfo.material_index;
//     return sample_triangle(pinfo, rands.xy, triangle_idx,
//     light.world_matrix);
// }

// TriangleRecord sample_area_light(const vec4 rands, const int num_lights,
//                                  out uint light_mesh_idx, out uint
//                                  triangle_idx, out uint material_idx) {
//     light_mesh_idx = uint(rands.x * num_lights);
//     Light light = lights[light_mesh_idx];
//     PrimMeshInfo pinfo = prim_info[light.prim_mesh_idx];
//     material_idx = pinfo.material_index;
//     triangle_idx = uint(rands.y * light.num_triangles);
//     return sample_triangle(pinfo, rands.zw, triangle_idx,
//     light.world_matrix);
// }

TriangleRecord sample_area_light(const vec4 rands, const int num_lights,
                                 const Light light, out uint triangle_idx,
                                 out uint material_idx) {
    // uint light_mesh_idx = uint(rands.x * num_lights);
    // light = lights[light_mesh_idx];
    // light_idx = light.prim_mesh_idx;
    PrimMeshInfo pinfo = prim_info[light.prim_mesh_idx];
    material_idx = pinfo.material_index;
    triangle_idx = uint(rands.y * light.num_triangles);
    return sample_triangle(pinfo, rands.zw, triangle_idx, light.world_matrix);
}

// TriangleRecord sample_area_light(out uint light_idx, out uint triangle_idx,
//                                  out uint material_idx, out Light light,
//                                  inout uvec4 seed, const int num_lights) {
//     const vec4 rands = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
//     return sample_area_light(rands, num_lights, light_idx, triangle_idx,
//                              material_idx, light);
// }

vec3 sample_light(const vec3 p, const int num_lights, out uint light_idx,
                  out uint triangle_idx, out uint material_idx, out Light light,
                  out TriangleRecord record, out MaterialProps light_mat,
                  inout uvec4 seed) {
    const vec4 rands = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    light_idx = uint(rands.x * num_lights);
    light = lights[light_idx];
    vec3 L = vec3(0);
    if (light.light_flags == LIGHT_AREA) {
        record = sample_area_light(rands, num_lights, light, triangle_idx,
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
    }
    return L;
}

vec3 uniform_sample_cone(vec2 uv, float cos_max) {
    const float cos_theta = (1. - uv.x) + uv.x * cos_max;
    const float sin_theta = sqrt(1 - cos_theta * cos_theta);
    const float phi = uv.y * PI2;
    return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

vec3 sample_light_Le(const int num_lights, const int total_light,
                     out uint light_idx, out uint triangle_idx,
                     out uint material_idx, out Light light,
                     out TriangleRecord record, out MaterialProps light_mat,
                     out float pdf_pos, inout uvec4 seed, out vec3 wi,
                     out float pdf_dir) {
    const vec4 rands = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    light_idx = uint(rands.x * num_lights);
    light = lights[light_idx];
    vec3 L = vec3(0);
    if (light.light_flags == LIGHT_AREA) {
        record = sample_area_light(rands, num_lights, light, triangle_idx,
                                   material_idx);
        vec2 uv_unused;
        light_mat = load_material(material_idx, uv_unused);
        pdf_pos = record.triangle_pdf / total_light;
        L = light_mat.emissive_factor;
        const vec2 rands_dir = vec2(rand(seed), rand(seed));
        wi = sample_cos_hemisphere(vec2(rand(seed), rand(seed)),
                                   record.triangle_normal);
        pdf_dir = abs(dot(wi, record.triangle_normal)) / PI;
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
        pdf_pos = 1.;
        pdf_dir = uniform_cone_pdf(cos_width);
    }

    return L;
}

#endif