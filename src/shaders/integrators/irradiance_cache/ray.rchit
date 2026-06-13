#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "ir_commons.h"

hitAttributeEXT vec2 attribs;
layout(location = 0) rayPayloadInEXT IRCacheHitPayload payload;

void main() {
    payload.triangle_idx = gl_PrimitiveID;
    payload.instance_idx = gl_InstanceCustomIndexEXT;
    payload.attribs = attribs;
    payload.dist = gl_RayTminEXT + gl_HitTEXT;
}
