#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../commons.h"

hitAttributeEXT vec2 attribs;
layout(location = 0) rayPayloadInEXT DDGISurfaceHitPayload payload;

void main() {
	payload.surface.barycentrics = attribs;
	payload.surface.primitive_instance_id = uvec2(gl_PrimitiveID, gl_InstanceCustomIndexEXT);
	payload.dist = gl_RayTminEXT + gl_HitTEXT;
	payload.hit_kind = gl_HitKindEXT;
}
