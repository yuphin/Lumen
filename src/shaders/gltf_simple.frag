#version 450

layout (location = 0) in vec2 in_uv0;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_view_vec;
layout (location = 3) in vec3 in_light_vec;

layout (location = 0) out vec4 out_color;

layout(push_constant) uniform MaterialPushConst {
	layout(offset = 64) vec4 base_color_factor;
};

void main()  {
	vec4 color;
	color = base_color_factor;
	const float ambient = 0.1;
	vec3 L = normalize(in_light_vec);
	vec3 V = normalize(in_view_vec);
	vec3 N = in_normal;
	vec3 R = reflect(-L, N);
	vec3 diffuse = max(dot(N, L), ambient).rrr * 0.5;
	float specular = pow(max(dot(R, V), 0.0), 32.0);
	vec3 final_col = diffuse * color.rgb + specular;
	out_color = vec4(final_col, color.a);
}