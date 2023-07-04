#ifndef COMMONS_DEVICE
#define COMMONS_DEVICE
#include "commons.h"
#include "utils.glsl"
#include "atmosphere/atmosphere.glsl"


#ifndef SCENE_TEX_IDX
#define SCENE_TEX_IDX 4
#endif

layout(binding = 0, rgba32f) uniform image2D image;
layout(binding = 1) uniform SceneUBOBuffer { SceneUBO ubo; };
layout(binding = 2) buffer SceneDesc_ { SceneDesc scene_desc; };
layout(binding = 3, scalar) readonly buffer Lights { Light lights[]; };
layout(binding = SCENE_TEX_IDX) uniform sampler2D scene_textures[];


layout(set = 1, binding = 0) uniform accelerationStructureEXT tlas;
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer InstanceInfo {
    PrimMeshInfo d[];
};
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Vertices { vec3 v[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Indices { uint i[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Normals { vec3 n[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer TexCoords { vec2 t[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Materials { Material m[]; };

Indices indices = Indices(scene_desc.index_addr);
Vertices vertices = Vertices(scene_desc.vertex_addr);
Normals normals = Normals(scene_desc.normal_addr);
Materials materials = Materials(scene_desc.material_addr);
InstanceInfo prim_infos = InstanceInfo(scene_desc.prim_info_addr);
TexCoords tex_coords = TexCoords(scene_desc.uv_addr);

#include "bsdf_commons.glsl"

vec4 sample_camera(in vec2 d) {
    vec4 target = ubo.inv_projection * vec4(d.x, d.y, 1, 1);
    return ubo.inv_view * vec4(normalize(target.xyz), 0); // direction
}

float correct_shading_normal(const vec3 n_g, const vec3 n_s, const vec3 wi,
                             const vec3 wo, int mode) {
    if (mode == 0) {
        float num = abs(dot(wo, n_s) * abs(dot(wi, n_g)));
        float denom = abs(dot(wo, n_g) * abs(dot(wi, n_s)));
        if (denom == 0)
            return 0.;
        return num / denom;
    } else {
        return 1.;
    }
}

float uniform_cone_pdf(float cos_max) { return 1. / (PI2 * (1 - cos_max)); }

bool is_light_finite(uint light_props) {
    return ((light_props >> 4) & 0x1) != 0;
}

bool is_light_delta(uint light_props) {
    return ((light_props >> 5) & 0x1) != 0;
}

uint get_light_type(uint light_props) { return uint(light_props & 0x7); }

float light_pdf(const Light light, const vec3 n_s, const vec3 wi) {
    const float cos_width = cos(30 * PI / 180);
    uint light_type = get_light_type(light.light_flags);
    switch (light_type) {
    case LIGHT_AREA: {
        return max(dot(n_s, wi) / PI, 0);
    } break;
    case LIGHT_SPOT: {
        return uniform_cone_pdf(cos_width);
    } break;
    case LIGHT_DIRECTIONAL: {
        return 0;
    } break;
    }
}

float light_pdf_a_to_w(const uint light_flags, const float pdf_a,
                       const vec3 n_s, const float wi_len_sqr,
                       const float cos_from_light) {
    uint light_type = get_light_type(light_flags);
    switch (light_type) {
    case LIGHT_AREA: {
        return pdf_a * wi_len_sqr / cos_from_light;
    } break;
    case LIGHT_SPOT: {
        return wi_len_sqr / cos_from_light;
    } break;
    case LIGHT_DIRECTIONAL: {
        return 1;
    } break;
    }
    return 0;
}

float light_pdf(uint light_flags, const vec3 n_s, const vec3 wi) {
    const float cos_width = cos(30 * PI / 180);
    uint light_type = get_light_type(light_flags);
    switch (light_type) {
    case LIGHT_AREA: {
        return max(dot(n_s, wi) / PI, 0);
    }
    case LIGHT_SPOT: {
        return uniform_cone_pdf(cos_width);
    }
    case LIGHT_DIRECTIONAL: {
        return 0;
    }
    }
}

float light_pdf_Le(uint light_flags, const vec3 n_s, const vec3 wi) {
    const float cos_width = cos(30 * PI / 180);
    switch (get_light_type(light_flags)) {
    case LIGHT_AREA: {
        return max(dot(n_s, wi) / PI, 0);
    }
    case LIGHT_SPOT: {
        return uniform_cone_pdf(cos_width);
    }
    case LIGHT_DIRECTIONAL: {
        return 1;
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
    const vec3 e0 = vec3(v2 - v0);
    const vec3 e1 = vec3(v1 - v0);
    result.n_s = normalize(vec3(inv_tr_mat * nrm));
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

vec3 shade_atmosphere(uint dir_light_idx, vec3 sky_col, vec3 ray_origin, vec3 ray_dir, float ray_length) {
    if(dir_light_idx == -1) {
        return sky_col;
    }
    Light light = lights[dir_light_idx];
    vec3 light_dir = -normalize(light.to - light.pos);
    vec3 transmittance;
    vec2 planet_isect = planet_intersection(ray_origin, ray_dir);
    if(planet_isect.x > 0) {
        ray_length = min(ray_length, planet_isect.x);
    }
    return integrate_scattering(ray_origin, ray_dir, ray_length, light_dir, light.L, transmittance);
}
/*
    Light sampling
*/

TriangleRecord sample_area_light(const vec4 rands, const int num_lights,
                                 const Light light, out uint triangle_idx,
                                 out uint material_idx) {
    PrimMeshInfo pinfo = prim_infos.d[light.prim_mesh_idx];
    material_idx = pinfo.material_index;
    triangle_idx = uint(rands.y * light.num_triangles);
    return sample_triangle(pinfo, rands.zw, triangle_idx, light.world_matrix);
}

TriangleRecord sample_area_light(const vec4 rands, const int num_lights,
                                 const Light light, out uint triangle_idx,
                                 out uint material_idx, out float u,
                                 out float v) {
    PrimMeshInfo pinfo = prim_infos.d[light.prim_mesh_idx];
    material_idx = pinfo.material_index;
    triangle_idx = uint(rands.y * light.num_triangles);
    return sample_triangle(pinfo, rands.zw, triangle_idx, light.world_matrix, u,
                           v);
}

TriangleRecord sample_area_light_with_idx(const vec4 rands,
                                          const int num_lights,
                                          const Light light,
                                          const uint triangle_idx,
                                          out uint material_idx) {
    PrimMeshInfo pinfo = prim_infos.d[light.prim_mesh_idx];
    material_idx = pinfo.material_index;
    return sample_triangle(pinfo, rands.zw, triangle_idx, light.world_matrix);
}

TriangleRecord sample_area_light(const vec4 rands, const Light light,
                                 out uint material_idx, out uint triangle_idx,
                                 out float u, out float v) {
    PrimMeshInfo pinfo = prim_infos.d[light.prim_mesh_idx];
    material_idx = pinfo.material_index;
    triangle_idx = uint(rands.y * light.num_triangles);
    return sample_triangle(pinfo, rands.zw, triangle_idx, light.world_matrix, u,
                           v);
}

TriangleRecord sample_area_light(const vec4 rands, const Light light,
                                 out uint material_idx, out uint triangle_idx) {
    PrimMeshInfo pinfo = prim_infos.d[light.prim_mesh_idx];
    material_idx = pinfo.material_index;
    triangle_idx = uint(rands.y * light.num_triangles);
    return sample_triangle(pinfo, rands.zw, triangle_idx, light.world_matrix);
}

TriangleRecord sample_area_light(const vec4 rands, const Light light) {
    PrimMeshInfo pinfo = prim_infos.d[light.prim_mesh_idx];
    uint triangle_idx = uint(rands.y * light.num_triangles);
    return sample_triangle(pinfo, rands.zw, triangle_idx, light.world_matrix);
}

vec3 uniform_sample_cone(vec2 uv, float cos_max) {
    const float cos_theta = (1. - uv.x) + uv.x * cos_max;
    const float sin_theta = sqrt(1 - cos_theta * cos_theta);
    const float phi = uv.y * PI2;
    return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

vec3 sample_light_Li(const vec4 rands_pos, const vec3 p, const int num_lights,
                     out vec3 wi, out float wi_len, out vec3 n, out vec3 pos,
                     out float pdf_pos_a, out float cos_from_light,
                     out LightRecord light_record) {

    uint light_idx = uint(rands_pos.x * num_lights);
    Light light = lights[light_idx];
    uint light_type = get_light_type(light.light_flags);
    vec3 L = vec3(0);
    switch (light_type) {
    case LIGHT_AREA: {
        vec2 uv_unused;
        uint material_idx;
        uint triangle_idx;
        TriangleRecord record =
            sample_area_light(rands_pos, light, material_idx, triangle_idx);
        Material light_mat = load_material(material_idx, uv_unused);
        wi = record.pos - p;
        float wi_len_sqr = dot(wi, wi);
        wi_len = sqrt(wi_len_sqr);
        wi /= wi_len;
        cos_from_light = max(dot(record.n_s, -wi), 0);
        L = light_mat.emissive_factor;
        pdf_pos_a = record.triangle_pdf;
        light_record.material_idx = material_idx;
        light_record.triangle_idx = triangle_idx;
        light_record.light_idx = light_idx;
        n = record.n_s;
        pos = record.pos;
    } break;
    case LIGHT_SPOT: {
        wi = light.pos - p;
        float wi_len_sqr = dot(wi, wi);
        wi_len = sqrt(wi_len_sqr);
        wi /= wi_len;
        const vec3 light_dir = normalize(light.to - light.pos);
        cos_from_light = dot(-wi, light_dir);
        const float cos_width = cos(PI / 6);
        const float cos_faloff = cos(25 * PI / 180);
        float faloff;
        if (cos_from_light < cos_width) {
            faloff = 0;
        } else if (cos_from_light >= cos_faloff) {
            faloff = 1;
        } else {
            float d = (cos_from_light - cos_width) / (cos_faloff - cos_width);
            faloff = (d * d) * (d * d);
        }
        pdf_pos_a = 1;
        L = light.L * faloff;
        n = -wi;
        pos = light.pos;
    } break;
    case LIGHT_DIRECTIONAL: {
        const vec3 dir = normalize(light.pos - light.to);
        const vec3 light_p = p + dir * (2 * light.world_radius);
        wi = light_p - p;
        wi_len = length(wi);
        wi /= wi_len;
        pdf_pos_a = 1;
        L = light.L;
        cos_from_light = 1.;
        n = -wi;
        pos = light_p;
    } break;
    default:
        break;
    }
    light_record.flags = light.light_flags;
    return L;
}

vec3 sample_light_Li(const vec4 rands_pos, const vec3 p, const int num_lights,
                     out float pdf_pos_w, out vec3 wi, out float wi_len,
                     out float pdf_pos_a, out float cos_from_light,
                     out LightRecord light_record) {
    uint light_idx = uint(rands_pos.x * num_lights);
    Light light = lights[light_idx];
    uint light_type = get_light_type(light.light_flags);
    vec3 L = vec3(0);
    switch (light_type) {
    case LIGHT_AREA: {
        vec2 uv_unused;
        uint material_idx;
        uint triangle_idx;
        TriangleRecord record =
            sample_area_light(rands_pos, light, material_idx, triangle_idx);
        Material light_mat = load_material(material_idx, uv_unused);
        wi = record.pos - p;
        float wi_len_sqr = dot(wi, wi);
        wi_len = sqrt(wi_len_sqr);
        wi /= wi_len;
        cos_from_light = max(dot(record.n_s, -wi), 0);
        L = light_mat.emissive_factor;
        pdf_pos_a = record.triangle_pdf;
        pdf_pos_w = pdf_pos_a * wi_len_sqr / cos_from_light;
        light_record.material_idx = material_idx;
        light_record.triangle_idx = triangle_idx;
    } break;
    case LIGHT_SPOT: {
        wi = light.pos - p;
        float wi_len_sqr = dot(wi, wi);
        wi_len = sqrt(wi_len_sqr);
        wi /= wi_len;
        const vec3 light_dir = normalize(light.to - light.pos);
        cos_from_light = dot(-wi, light_dir);
        const float cos_width = cos(PI / 6);
        const float cos_faloff = cos(25 * PI / 180);
        float faloff;
        if (cos_from_light < cos_width) {
            faloff = 0;
        } else if (cos_from_light >= cos_faloff) {
            faloff = 1;
        } else {
            float d = (cos_from_light - cos_width) / (cos_faloff - cos_width);
            faloff = (d * d) * (d * d);
        }
        pdf_pos_a = 1;
        pdf_pos_w = wi_len_sqr;
        L = light.L * faloff;
    } break;
    case LIGHT_DIRECTIONAL: {
        const vec3 dir = normalize(light.pos - light.to);
        const vec3 light_p = p + dir * (2 * light.world_radius);
        wi = light_p - p;
        wi_len = length(wi);
        wi /= wi_len;
        pdf_pos_a = 1;
        pdf_pos_w = 1;
        L = light.L;
        cos_from_light = 1.;
    } break;
    default:
        break;
    }
    light_record.flags = light.light_flags;
    return L;
}

vec3 sample_light_Li(const vec4 rands_pos, const vec3 p, const int num_lights,
                     out vec3 wi, out float wi_len, out float pdf_pos_w,
                     out float pdf_pos_dir_w, out float cos_from_light,
                     out LightRecord light_record) {
    uint light_idx = uint(rands_pos.x * num_lights);
    Light light = lights[light_idx];
    uint light_type = get_light_type(light.light_flags);
    vec3 L = vec3(0);
    switch (light_type) {
    case LIGHT_AREA: {
        vec2 uv_unused;
        uint material_idx;
        uint triangle_idx;
        TriangleRecord record =
            sample_area_light(rands_pos, light, material_idx, triangle_idx);
        Material light_mat = load_material(material_idx, uv_unused);
        wi = record.pos - p;
        float wi_len_sqr = dot(wi, wi);
        wi_len = sqrt(wi_len_sqr);
        wi /= wi_len;
        cos_from_light = max(dot(record.n_s, -wi), 0);
        L = light_mat.emissive_factor;
        pdf_pos_w = record.triangle_pdf * wi_len_sqr / cos_from_light;
        pdf_pos_dir_w = cos_from_light * INV_PI * record.triangle_pdf;
        light_record.material_idx = material_idx;
        light_record.triangle_idx = triangle_idx;
    } break;
    case LIGHT_SPOT: {
        wi = light.pos - p;
        float wi_len_sqr = dot(wi, wi);
        wi_len = sqrt(wi_len_sqr);
        wi /= wi_len;
        const vec3 light_dir = normalize(light.to - light.pos);
        cos_from_light = dot(-wi, light_dir);
        const float cos_width = cos(PI / 6);
        const float cos_faloff = cos(25 * PI / 180);
        float faloff;
        if (cos_from_light < cos_width) {
            faloff = 0;
        } else if (cos_from_light >= cos_faloff) {
            faloff = 1;
        } else {
            float d = (cos_from_light - cos_width) / (cos_faloff - cos_width);
            faloff = (d * d) * (d * d);
        }
        pdf_pos_w = wi_len_sqr / cos_from_light;
        pdf_pos_dir_w = uniform_cone_pdf(cos_width);
        L = light.L * faloff;
    } break;
    case LIGHT_DIRECTIONAL: {
        const vec3 dir = normalize(light.pos - light.to);
        const vec3 light_p = p + dir * (2 * light.world_radius);
        wi = light_p - p;
        wi_len = length(wi);
        wi /= wi_len;
        pdf_pos_w = 1;
        pdf_pos_dir_w = INV_PI / (light.world_radius * light.world_radius);
        L = light.L;
        cos_from_light = 1.;
    } break;
    default:
        break;
    }
    light_record.flags = light.light_flags;
    return L;
}

vec3 sample_light_Li(inout uvec4 seed, const vec3 p, const int num_lights,
                     out vec3 wi, out float wi_len, out float pdf_pos_w,
                     out float pdf_pos_dir_w, out LightRecord record,
                     out float cos_from_light) {
    const vec4 rands = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    return sample_light_Li(rands, p, num_lights, wi, wi_len, pdf_pos_w,
                           pdf_pos_dir_w, cos_from_light, record);
}

vec3 sample_light_Li(inout uvec4 seed, const vec3 p, const int num_lights,
                     out float pdf_pos_w, out vec3 wi, out float wi_len,
                     out float pdf_pos_a, out float cos_from_light,
                     out LightRecord record) {
    const vec4 rands = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    return sample_light_Li(rands, p, num_lights, pdf_pos_w, wi, wi_len,
                           pdf_pos_a, cos_from_light, record);
}

vec3 sample_light_Li(inout uvec4 seed, const vec3 p, const int num_lights,
                     out vec3 wi, out float wi_len, out vec3 n, out vec3 pos,
                     out float pdf_pos_a, out float cos_from_light,
                     out LightRecord light_record) {
    const vec4 rands_pos = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    return sample_light_Li(rands_pos, p, num_lights, wi, wi_len, n, pos,
                           pdf_pos_a, cos_from_light, light_record);
}

vec3 sample_light_Le(const vec4 rands_pos, const vec2 rands_dir,
                     const int num_lights, const int total_light,
                     out float cos_from_light, out LightRecord light_record,
                     out vec3 pos, out vec3 wi, out vec3 n, out float pdf_pos_a,
                     out float pdf_dir_w, out float pdf_emit_w,
                     out float pdf_direct_a, out float phi, out float u,
                     out float v) {
    uint light_idx = uint(rands_pos.x * num_lights);
    Light light = lights[light_idx];
    vec3 L = vec3(0);
    uint light_type = get_light_type(light.light_flags);
    switch (light_type) {
    case LIGHT_AREA: {
        vec2 uv_unused;
        uint material_idx;
        uint triangle_idx;
        TriangleRecord record = sample_area_light(
            rands_pos, light, material_idx, triangle_idx, u, v);
        Material light_mat = load_material(material_idx, uv_unused);
        pos = record.pos;
        wi = sample_cos_hemisphere(rands_dir, record.n_s, phi);
        n = record.n_s;
        L = light_mat.emissive_factor;
        cos_from_light = max(dot(record.n_s, wi), 0);
        pdf_pos_a = record.triangle_pdf;
        pdf_dir_w = (dot(wi, record.n_s)) / PI;
        pdf_emit_w = pdf_pos_a * pdf_dir_w;
        pdf_direct_a = pdf_pos_a;
        light_record.material_idx = material_idx;
        light_record.triangle_idx = triangle_idx;
        light_record.light_idx = light_idx;
    } break;
    case LIGHT_SPOT: {
        const float cos_width = cos(30 * PI / 180);
        const float cos_faloff = cos(25 * PI / 180);
        const vec3 light_dir = normalize(light.to - light.pos);
        vec4 local_quat = to_local_quat(light_dir);
        wi = rot_quat(invert_quat(local_quat),
                      uniform_sample_cone(rands_dir, cos_width));
        pos = light.pos;
        cos_from_light = dot(wi, light_dir);
        float faloff;
        if (cos_from_light < cos_width) {
            faloff = 0;
        } else if (cos_from_light >= cos_faloff) {
            faloff = 1;
        } else {
            float d = (cos_from_light - cos_width) / (cos_faloff - cos_width);
            faloff = (d * d) * (d * d);
        }
        L = light.L * faloff;
        pdf_pos_a = 1.;
        pdf_dir_w = uniform_cone_pdf(cos_width);
        pdf_emit_w = pdf_pos_a * pdf_dir_w;
        pdf_direct_a = pdf_pos_a;
        u = 0;
        v = 0;
        n = wi;

    } break;
    case LIGHT_DIRECTIONAL: {
        vec3 dir = -normalize(light.to - light.pos);
        vec3 v1, v2;
        make_coord_system(dir, v1, v2);
        vec2 uv = concentric_sample_disk(rands_dir);
        vec3 l_pos =
            light.world_center + light.world_radius * (uv.x * v1 + uv.y * v2);
        pos = l_pos + dir * light.world_radius;
        wi = -dir;
        L = light.L;
        pdf_pos_a = 1. / (PI * light.world_radius * light.world_radius);
        pdf_dir_w = 1;
        pdf_emit_w = pdf_pos_a;
        pdf_direct_a = 1.;
        cos_from_light = 1;
        u = 0;
        v = 0;
        n = wi;
    } break;
    default:
        break;
    }
    pdf_pos_a /= total_light;
    light_record.flags = light.light_flags;
    return L;
}

vec3 sample_light_Le(inout uvec4 seed, const int num_lights,
                     const int total_light, out float cos_from_light,
                     out LightRecord light_record, out vec3 pos, out vec3 wi,
                     out float pdf_pos_a, out float pdf_dir_w, out float phi,
                     out float u, out float v) {
    const vec4 rands_pos = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    const vec2 rands_dir = vec2(rand(seed), rand(seed));
    vec3 n;
    float pdf_emit_w, pdf_direct_a;
    return sample_light_Le(rands_pos, rands_dir, num_lights, total_light,
                           cos_from_light, light_record, pos, wi, n, pdf_pos_a,
                           pdf_dir_w, pdf_emit_w, pdf_direct_a, phi, u, v);
}

vec3 sample_light_Le(const vec4 rands_pos, const vec2 rands_dir,
                     const int num_lights, const int total_light,
                     out float cos_from_light, out LightRecord light_record,
                     out vec3 pos, out vec3 wi, out float pdf_pos_a,
                     out float pdf_dir_w, out float pdf_emit_w,
                     out float pdf_direct_a) {
    float phi, u, v;
    vec3 n;
    return sample_light_Le(rands_pos, rands_dir, num_lights, total_light,
                           cos_from_light, light_record, pos, wi, n, pdf_pos_a,
                           pdf_dir_w, pdf_emit_w, pdf_direct_a, phi, u, v);
}

vec3 sample_light_Le(inout uvec4 seed, const int num_lights,
                     const int total_light, out float cos_from_light,
                     out LightRecord light_record, out vec3 pos, out vec3 wi,
                     out float pdf_pos_a, out float pdf_dir_w,
                     out float pdf_emit_w, out float pdf_direct_a) {
    const vec4 rands_pos = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    const vec2 rands_dir = vec2(rand(seed), rand(seed));
    float phi, u, v;
    vec3 n;
    return sample_light_Le(rands_pos, rands_dir, num_lights, total_light,
                           cos_from_light, light_record, pos, wi, n, pdf_pos_a,
                           pdf_dir_w, pdf_emit_w, pdf_direct_a, phi, u, v);
}

vec3 sample_light_Le(inout uvec4 seed, const int num_lights,
                     const int total_light, out float cos_from_light,
                     out LightRecord light_record, out vec3 pos, out vec3 wi,
                     out float pdf_pos_a, out float pdf_dir_w) {
    const vec4 rands_pos = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    const vec2 rands_dir = vec2(rand(seed), rand(seed));
    float phi, u, v;
    vec3 n;
    float pdf_emit_w, pdf_direct_a;
    return sample_light_Le(rands_pos, rands_dir, num_lights, total_light,
                           cos_from_light, light_record, pos, wi, n, pdf_pos_a,
                           pdf_dir_w, pdf_emit_w, pdf_direct_a, phi, u, v);
}

vec3 sample_light_Le(const int num_lights, const int total_light,
                     out float cos_from_light, out LightRecord light_record,
                     out vec3 pos, out vec3 wi, out vec3 n, out float pdf_pos_a,
                     out float pdf_dir_w, const vec4 rands_pos,
                     const vec2 rands_dir) {
    float phi, u, v;
    float pdf_emit_w, pdf_direct_a;
    return sample_light_Le(rands_pos, rands_dir, num_lights, total_light,
                           cos_from_light, light_record, pos, wi, n, pdf_pos_a,
                           pdf_dir_w, pdf_emit_w, pdf_direct_a, phi, u, v);
}

vec3 sample_light_Le(inout uvec4 seed, const int num_lights,
                     const int total_light, out float cos_from_light,
                     out LightRecord light_record, out vec3 pos, out vec3 wi,
                     out vec3 n, out float pdf_pos_a, out float pdf_dir_w) {
    const vec4 rands_pos = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
    const vec2 rands_dir = vec2(rand(seed), rand(seed));
    float phi, u, v;
    float pdf_emit_w, pdf_direct_a;
    return sample_light_Le(rands_pos, rands_dir, num_lights, total_light,
                           cos_from_light, light_record, pos, wi, n, pdf_pos_a,
                           pdf_dir_w, pdf_emit_w, pdf_direct_a, phi, u, v);
}

vec3 sample_light_with_idx(const vec4 rands_pos, const vec3 p,
                           const int num_lights, const uint light_idx,
                           const uint triangle_idx, out vec3 pos, out vec3 n) {
    Light light = lights[light_idx];
    vec3 L = vec3(0);
    uint light_type = get_light_type(light.light_flags);
    if (light_type == LIGHT_AREA) {
        uint material_idx;
        vec2 uv_unused;
        TriangleRecord record = sample_area_light_with_idx(
            rands_pos, num_lights, light, triangle_idx, material_idx);
        Material light_mat = load_material(material_idx, uv_unused);
        L = light_mat.emissive_factor;
        pos = record.pos;
        n = record.n_s;
    } else if (light_type == LIGHT_SPOT) {
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
        pos = light.pos;
        n = dir;
        L = light.L * faloff;
    } else if (light_type == LIGHT_DIRECTIONAL) {
        vec3 dir = -normalize(light.to - light.pos);
        pos = p + dir * (2 * light.world_radius);
        n = -dir;
        L = light.L;
    }
    return L;
}
#endif