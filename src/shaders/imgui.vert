#version 450

layout(push_constant) uniform PushConstants {
	vec2 scale;
	vec2 translate;
} pc;

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;

void main() {
	out_uv = in_uv;
	out_color = in_color;
	gl_Position = vec4(in_position * pc.scale + pc.translate, 0.0, 1.0);
}
