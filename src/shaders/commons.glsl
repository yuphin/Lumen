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
layout(set = 0, binding = 6) readonly buffer AreaLights {
    MeshLight area_lights[];
};

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
    case BSDF_LAMBERTIAN:
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
    default:
        break;
    }
    res.ior = mat.ior;
    return res;
}

vec3 sample_bsdf(const vec3 shading_nrm, const vec3 wo, const MaterialProps mat,
                 const uint mode, const bool side, out vec3 dir,
                 out float pdf_w, inout float cos_theta, const vec2 rands) {
    vec3 f;
    switch (mat.bsdf_type) {
    case BSDF_LAMBERTIAN: {
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
    case BSDF_LAMBERTIAN: {
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
    case BSDF_LAMBERTIAN: {
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
    default: // Unknown
        break;
    }

    return f;
}

vec3 eval_bsdf(const MaterialProps mat, const vec3 wo, const vec3 wi) {
    if ((mat.bsdf_props & BSDF_LAMBERTIAN) == BSDF_LAMBERTIAN) {
        return mat.albedo / PI;
    } else if ((mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR) {
        return vec3(0);
    }
    return vec3(0);
}

vec3 eval_bsdf_selective(const vec3 shading_nrm, const vec3 wo,
                         const MaterialProps mat, const uint mode,
                         const bool side, const uint bsdf_props, const vec3 dir,
                         out float pdf_w, in float cos_theta) {
    if ((bsdf_props & BSDF_ALL) != 0) {
        return eval_bsdf(shading_nrm, wo, mat, mode, side, dir, pdf_w,
                         cos_theta);
    } else if (bsdf_props == (BSDF_ALL & ~BSDF_SPECULAR)) {
        if (mat.bsdf_type == BSDF_LAMBERTIAN) {
            vec3 f = mat.albedo / PI;
            pdf_w = max(cos_theta / PI, 0);
            return f;
        }
    }
    return vec3(0);
}

vec3 sample_bsdf_selective(const vec3 shading_nrm, const vec3 wo,
                           const MaterialProps mat, const uint mode,
                           const bool side, const uint bsdf_props, out vec3 dir,
                           out float pdf_w, out float cos_theta,
                           inout uvec4 seed) {
    if ((bsdf_props & BSDF_ALL) != 0) {
        return sample_bsdf(shading_nrm, wo, mat, mode, side, dir, pdf_w,
                           cos_theta, seed);
    } else if (bsdf_props == (BSDF_ALL & ~BSDF_SPECULAR)) {
        if (mat.bsdf_type == BSDF_LAMBERTIAN) {
            vec3 f = mat.albedo / PI;
            dir = sample_cos_hemisphere(vec2(rand(seed), rand(seed)),
                                        shading_nrm);
            cos_theta = dot(shading_nrm, dir);
            pdf_w = max(cos_theta / PI, 0);
            return f;
        }
    }
    return vec3(0);
}

float bsdf_pdf(const MaterialProps mat, const vec3 shading_nrm, const vec3 wo,
               const vec3 wi) {
    if ((mat.bsdf_props & BSDF_LAMBERTIAN) == BSDF_LAMBERTIAN) {
        return max(dot(shading_nrm, wi) / PI, 0);
    } else if ((mat.bsdf_props & BSDF_SPECULAR) == BSDF_SPECULAR) {
        return 0;
    }
    return 0;
}

TriangleRecord sample_triangle(PrimMeshInfo pinfo, vec2 rands,
                               uint triangle_idx, in mat4 world_matrix) {
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
    return result;
}

vec3 eval_albedo(const Material m) {
    vec3 albedo = m.base_color_factor.xyz;
    if (m.texture_id > -1) {
        albedo *= texture(scene_textures[m.texture_id], payload.uv).xyz;
    }
    return albedo;
}

TriangleRecord sample_area_light_with_idx(const vec4 rands,
                                          const uint light_mesh_idx,
                                          const uint triangle_idx,
                                          out uint material_idx) {
    const MeshLight light = area_lights[light_mesh_idx];
    PrimMeshInfo pinfo = prim_info[light.prim_mesh_idx];
    material_idx = pinfo.material_index;
    return sample_triangle(pinfo, rands.zw, triangle_idx, light.world_matrix);
}

TriangleRecord sample_area_light(const vec4 rands, const int num_mesh_lights,
                                 out uint light_mesh_idx, out uint triangle_idx,
                                 out uint material_idx) {
    light_mesh_idx = uint(rands.x * num_mesh_lights);
    MeshLight light = area_lights[light_mesh_idx];
    PrimMeshInfo pinfo = prim_info[light.prim_mesh_idx];
    material_idx = pinfo.material_index;
    triangle_idx = uint(rands.y * light.num_triangles);
    return sample_triangle(pinfo, rands.zw, triangle_idx, light.world_matrix);
}

TriangleRecord sample_area_light(const vec4 rands, const int num_mesh_lights,
                                 out uint light_idx, out uint triangle_idx,
                                 out uint material_idx, out MeshLight light) {
    uint light_mesh_idx = uint(rands.x * num_mesh_lights);
    light = area_lights[light_mesh_idx];
    light_idx = light.prim_mesh_idx;
    PrimMeshInfo pinfo = prim_info[light.prim_mesh_idx];
    material_idx = pinfo.material_index;
    triangle_idx = uint(rands.y * light.num_triangles);
    return sample_triangle(pinfo, rands.zw, triangle_idx, light.world_matrix);
}

TriangleRecord sample_area_light(out uint light_idx, out uint triangle_idx,
                                 out uint material_idx, out MeshLight light,
                                 inout uvec4 seed, const int num_mesh_lights) {
    const vec4 rands = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    return sample_area_light(rands, num_mesh_lights, light_idx, triangle_idx,
                             material_idx, light);
}

#endif