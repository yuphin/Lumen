#version 450
#extension GL_EXT_debug_printf : enable
layout(location = 0) out vec2 out_uv;
layout(set = 0, binding = 1) uniform sampler2D lena;
void main() {
    out_uv = vec2(gl_VertexIndex & 1, gl_VertexIndex >> 1);
    gl_Position = vec4(out_uv * 2.0f - 1.0f, 1.0f, 1.0f);
}
