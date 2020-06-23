#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

vec4 getPos(vec3 inPosition){
	return vec4(inPosition,1.0);
}
void main() {
    gl_Position = getPos(inPosition);
    fragColor = inColor;
}
