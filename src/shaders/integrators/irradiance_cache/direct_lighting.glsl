#ifndef IR_DIRECT_LIGHTING_GLSL
#define IR_DIRECT_LIGHTING_GLSL

#include "../../shadow_ray.glsl"

vec3 sample_direct_light(inout uvec4 seed, vec3 pos, const vec3 n_s, vec3 wo, Material hit_material,
						 bool forward_facing) {
	if (pc.num_lights == 0) {
		return vec3(0);
	}

	const LightLiSample light_sample = sample_light_Li(rand4(seed), pos, pc.num_lights);
	if (light_sample.distance <= EPS || light_sample.pdf_position_w <= 0.0) {
		return vec3(0);
	}

	const vec3 p = offset_ray2(pos, n_s);
	const float cos_x = dot(n_s, light_sample.wi);
	const vec3 f = eval_bsdf(hit_material, wo, light_sample.wi, n_s, /*mode=*/1,
							 /*forward_facing=*/forward_facing);

	if (!connection_occluded(p, light_sample.wi, light_sample.distance, 0x1)) {
		return f * abs(cos_x) * light_sample.Li / light_sample.pdf_position_w;
	}
	return vec3(0);
}

#endif
