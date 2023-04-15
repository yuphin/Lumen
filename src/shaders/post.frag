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
layout(set = 0, binding = 0) uniform sampler2D bloom_img;
layout(set = 0, binding = 1) uniform sampler2D input_img;
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
    vec4 input_tex = texture(input_img, in_uv).rgba;
    vec4 img;
    if(pc.enable_bloom == 1) {
      vec4 bloom_tex = texelFetch(bloom_img, (textureSize(bloom_img, 0).xy - ivec2(pc.width, pc.height)) / 2 + ivec2(gl_FragCoord.xy), 0);
      img = mix(input_tex, bloom_tex * pc.bloom_exposure, pc.bloom_amount);
    } else {
      img = input_tex;
    }

    if(pc.enable_tonemapping == 1) {
        img = vec4(aces(img.rgb), img.a);
    }
    fragColor =  img;
}
