#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../commons.h"

layout(location = 0) rayPayloadInEXT DDGISurfaceHitPayload payload;

void main() {
	payload.surface.barycentrics = vec2(0.0);
	payload.surface.primitive_instance_id = uvec2(INVALID_SURFACE_ID);
	payload.dist = 0.0;
	payload.hit_kind = 0u;
}
