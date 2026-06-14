#version 450

layout(set = 0, binding = 0) uniform sampler2D image;

layout(location = 0) in vec2 in_uv;
layout(location = 1) in vec4 in_color;

layout(location = 0) out vec4 out_color;

void main() {
    vec4 tex_color = in_color * texture(image, in_uv);
    vec3 linearized = pow(tex_color.rgb, vec3(2.2));
    out_color = vec4(linearized, tex_color.a);
}