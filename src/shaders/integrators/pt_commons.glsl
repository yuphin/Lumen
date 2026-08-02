#ifndef PT_COMMONS
#define PT_COMMONS

#include "../shadow_ray.glsl"
vec3 sample_direct_light(inout uvec4 seed, const Material mat, vec3 pos, const bool side, const vec3 n_s,
						 const vec3 n_g, vec3 wo, bool use_mis, out bool visible) {
	visible = false;
	if (pc.num_lights == 0) {
		return vec3(0.0);
	}

	const LightLiSample light_sample = sample_light_Li(rand4(seed), pos, pc.num_lights);
	if (light_sample.distance <= EPS || light_sample.pdf_position_w <= 0.0) {
		return vec3(0.0);
	}

	float bsdf_pdf;
	const float cos_x = dot(n_s, light_sample.wi);
	const vec3 f = eval_bsdf(n_s, n_g, wo, mat, TRANSPORT_MODE_FROM_CAMERA, side, light_sample.wi, bsdf_pdf);
	visible = !connection_occluded(offset_ray2(pos, n_g), light_sample.wi, light_sample.distance, 0x1);
	if (!visible) {
		return vec3(0.0);
	}

	float mis_weight = 1.0;
	if (use_mis && !is_light_delta(light_sample.flags)) {
		mis_weight = light_sample.pdf_position_w / (light_sample.pdf_position_w + bsdf_pdf);
	}
	return mis_weight * f * abs(cos_x) * light_sample.Li / light_sample.pdf_position_w;
}

vec3 sample_direct_light(inout uvec4 seed, const Material mat, vec3 pos, const bool side, const vec3 n_s,
						 const vec3 n_g, vec3 wo, bool use_mis) {
	bool unused_visible;
	return sample_direct_light(seed, mat, pos, side, n_s, n_g, wo, use_mis, unused_visible);
}

#endif
