#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_debug_printf : enable
#include "commons.h"
#include "utils.glsl"


layout(location = 1) rayPayloadInEXT AnyHitPayload payload;

void main() { payload.hit = 0; }