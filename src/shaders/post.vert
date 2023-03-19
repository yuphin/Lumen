#version 450
layout(location = 0) out vec2 out_uv;
layout(set = 0, binding = 1) uniform sampler2D lena;
void main() {
    out_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(out_uv * 2.0f - 1.0f, 1.0f, 1.0f);
    vec2 ratio = vec2(1600, 900) / vec2(textureSize(lena, 0).xy);
 
    // out_uv *= ratio;
}
