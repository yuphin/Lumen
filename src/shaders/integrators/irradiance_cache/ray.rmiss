#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#extension GL_EXT_debug_printf : enable
#include "ir_commons.h"

layout(location = 0) rayPayloadInEXT IRCacheHitPayload payload;

void main() { payload.instance_idx = -1; }