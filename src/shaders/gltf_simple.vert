#version 450
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv0;
#include "commons.h"
layout(set = 0, binding = 0) uniform SceneUBOBuf { SceneUBO uboScene; };
layout(push_constant) uniform PushConsts { mat4 model; };
layout(location = 0) out vec2 out_uv0;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec3 out_view_vec;
layout(location = 3) out vec3 out_light_vec;

void main() {
    out_uv0 = in_uv0;
    out_normal =
        (transpose(inverse(uboScene.model)) * vec4(in_normal, 1.0)).xyz;
    gl_Position =
        uboScene.projection * uboScene.view * model * vec4(in_pos.xyz, 1.0);
    vec4 pos = uboScene.model * vec4(in_pos, 1.0);
    out_light_vec = uboScene.light_pos.xyz - pos.xyz;
    out_view_vec = uboScene.view_pos.xyz - pos.xyz;
}