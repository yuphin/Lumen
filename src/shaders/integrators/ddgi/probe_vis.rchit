#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "../../commons.h"
#include "ddgi_commons.h"
#include "ddgi_utils.glsl"

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT DDGIVisualizationHitPayload payload;

layout(set = 0, binding = 2, scalar) buffer SceneDesc_ { SphereDesc scene_desc; };
layout(set = 1, binding = 0) uniform accelerationStructureEXT tlas;
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Vertices { SphereVertex d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Indices { uint i[]; };

void main() {
    Indices indices = Indices(scene_desc.index_addr);
    Vertices vertices = Vertices(scene_desc.vertex_addr);

    const uint index_offset = 3 * gl_PrimitiveID;
    ivec3 ind = ivec3(indices.i[index_offset + 0], indices.i[index_offset + 1], indices.i[index_offset + 2]);

    SphereVertex vtx[3];
    vtx[0] = vertices.d[ind.x];
    vtx[1] = vertices.d[ind.y];
    vtx[2] = vertices.d[ind.z];


	const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    const vec3 local_normal = normalize(vtx[0].normal * barycentrics.x + vtx[1].normal * barycentrics.y + vtx[2].normal * barycentrics.z);
    const vec3 world_normal = normalize(vec3(local_normal * gl_WorldToObjectEXT));

    const vec3 local_pos = vtx[0].pos * barycentrics.x + vtx[1].pos * barycentrics.y + vtx[2].pos * barycentrics.z;
    const vec3 world_pos = vec3(gl_ObjectToWorldEXT * vec4(local_pos, 1.0));

    payload.n_s = world_normal;
    payload.pos = world_pos;
}