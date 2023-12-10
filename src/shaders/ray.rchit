#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "commons.h"
#include "utils.glsl"


hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT HitPayload payload;

layout(set = 0, binding = 2, scalar) buffer SceneDesc_ {
    SceneDesc scene_desc;
};
layout(set = 1, binding = 0) uniform accelerationStructureEXT tlas;
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer InstanceInfo {
    PrimMeshInfo prim_info[];
};
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer CompactVertices { Vertex d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Indices { uint i[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Materials {
    Material m[];
};

void main() {
    
    // Object data
    Materials materials = Materials(scene_desc.material_addr);
    Indices indices = Indices(scene_desc.index_addr);
    InstanceInfo prim_infos = InstanceInfo(scene_desc.prim_info_addr);
    CompactVertices compact_vertices = CompactVertices(scene_desc.compact_vertices_addr);

    PrimMeshInfo pinfo = prim_infos.prim_info[gl_InstanceCustomIndexEXT];
    // Getting the 'first index' for this mesh (offset of the mesh + offset of
    // the triangle)
    uint index_offset = pinfo.index_offset + 3 * gl_PrimitiveID;
    uint vertex_offset =
        pinfo.vertex_offset; // Vertex offset as defined in glTF
    uint material_index = pinfo.material_index; // material of primitive mesh


    // Indices of the triangle
    ivec3 ind = ivec3(indices.i[index_offset + 0], indices.i[index_offset + 1],
                      indices.i[index_offset + 2]);

    ind += ivec3(vertex_offset);
    // Vertex of the triangle
    Vertex vtx[3];
	vtx[0] = compact_vertices.d[ind.x];
	vtx[1] = compact_vertices.d[ind.y];
	vtx[2] = compact_vertices.d[ind.z];
	const vec3 v0 = vtx[0].pos;
	const vec3 v1 = vtx[1].pos;
	const vec3 v2 = vtx[2].pos;
	const vec3 n0 = vtx[0].normal;
	const vec3 n1 = vtx[1].normal;
	const vec3 n2 = vtx[2].normal;

    const vec2 uv0 = vtx[0].uv0;
    const vec2 uv1 = vtx[1].uv0;
    const vec2 uv2 = vtx[2].uv0;
    const vec3 barycentrics =
        vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    // Computing the coordinates of the hit position
    const vec3 pos =
        v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
    const vec3 world_pos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));

    // Computing the normal at hit position
    const vec3 nrm = normalize(n0 * barycentrics.x + n1 * barycentrics.y +
                               n2 * barycentrics.z);
    // Note that this is the transpose of the inverse of gl_ObjectToWorldEXT
    const vec3 world_nrm = normalize(vec3(nrm * gl_WorldToObjectEXT));

    const vec2 uv =
        uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

    const vec3 e0 = v2 - v0;
    const vec3 e1 = v1 - v0;
    const vec3 e0t = gl_ObjectToWorldEXT * vec4(e0,0);
    const vec3 e1t = gl_ObjectToWorldEXT * vec4(e1,0);


    payload.n_g = normalize(vec3(cross(e0, e1) * gl_WorldToObjectEXT));
    payload.n_s = world_nrm;
    payload.pos = world_pos;
    payload.uv = uv;
    payload.material_idx = material_index;
    payload.triangle_idx = gl_PrimitiveID;
    payload.area = 0.5 * length(cross(e0t, e1t));
    payload.dist = gl_RayTminEXT + gl_HitTEXT;
    payload.hit_kind = gl_HitKindEXT;
}