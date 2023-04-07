#version 450
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_atomic_float : require
#include "commons.h"
layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 fragColor;
layout(set = 0, binding = 0) uniform sampler2D input_img;
layout(set = 0, binding = 1) uniform sampler2D kernel;
layout(push_constant) uniform PCPost_ {  PCPost pc; };

vec3 aces(vec3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float aces(float x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    // vec4 img = texture(input_img, in_uv).rgba;
    vec2 uv_offset = vec2(textureSize(input_img, 0).xy - ivec2(1600, 900)) / vec2(2 * textureSize(input_img, 0).xy);
    uv_offset.y *= -1;
    vec4 bloom_tex = texelFetch(input_img, (textureSize(input_img, 0).xy - ivec2(1600, 900)) / 2 + ivec2(gl_FragCoord.xy), 0);
    vec4 img = 0.0001 * bloom_tex;
//
////    float abs_val = (sqrt(img.z * img.z + img.w * img.w));
//    float abs_val = (sqrt(img.x * img.x + img.y * img.y));
//
//    // img = 0.00001 * abs_val * vec4(1);
    // img = abs(img);
    
    
    // img.a = 1;
    if(pc.enable_tonemapping == 1) {
        img = vec4(aces(img.rgb), img.a);
    }
    fragColor =  img;
}
