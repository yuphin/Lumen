#version 450
layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 fragColor;
layout(set = 0, binding = 0) uniform sampler2D input_img;

void main() {
    vec2 uv = in_uv;
    fragColor = texture(input_img, uv).rgba;
}
