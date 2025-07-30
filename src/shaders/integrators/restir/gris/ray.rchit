#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "gris_commons.h"

hitAttributeEXT vec2 attribs;
layout(location = 0) rayPayloadInEXT GrisHitPayload payload;
layout(set = 0, binding = 2, scalar) buffer SceneDesc_ {
    SceneDesc scene_desc;
};
layout(set = 1, binding = 0) uniform accelerationStructureEXT tlas;

void main() {
    payload.triangle_idx = gl_PrimitiveID;
    payload.instance_idx = gl_InstanceCustomIndexEXT;
    payload.attribs = attribs;
    payload.dist = gl_RayTminEXT + gl_HitTEXT;
}