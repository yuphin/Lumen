#version 450

layout (set = 1, binding = 0) uniform sampler2D samplerColorMap;

layout (location = 0) in vec2 inUV0;
layout (location = 1) in vec2 inUV1;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inColor;
layout (location = 4) in vec4 inTangent;

layout (location = 5) in vec3 inViewVec;
layout (location = 6) in vec3 inLightVec;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform MaterialPushConst {
	vec4 base_color_factor;
	int base_color_uv_set;
} mat_const;

void main()  {

	vec4 color;
	if(mat_const.base_color_uv_set > -1){
		color = texture(samplerColorMap, inUV0);
	}else{
		color = mat_const.base_color_factor;
	}
	const float ambient = 0.1;
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
	vec3 N = inNormal;
	vec3 R = reflect(-L, N);
	vec3 diffuse = max(dot(N, L), ambient).rrr * 0.5;
	float specular = pow(max(dot(R, V), 0.0), 32.0);
	outColor = vec4(ambient + diffuse + specular + color.rgb, 1.0);
}