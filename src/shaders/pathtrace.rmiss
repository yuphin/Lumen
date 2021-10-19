#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "raystructs.glsl"
#include "commons.h"

layout(location = 0) rayPayloadInEXT HitPayload payload;

layout(push_constant) uniform _PushConstantRay
{
  PushConstantRay pcRay;
};

void main() {
  payload.material_idx = -1;
}