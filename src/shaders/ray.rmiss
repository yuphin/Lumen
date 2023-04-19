#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#extension GL_EXT_debug_printf : enable
#include "commons.h"
#include "utils.glsl"


layout(location = 0) rayPayloadInEXT HitPayload payload;

void main() { payload.material_idx = -1; }