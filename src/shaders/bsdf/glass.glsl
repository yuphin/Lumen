#ifndef GLASS_GLSL
#define GLASS_GLSL
#include "sampling_commons.glsl"
vec3 sample_glass(Material mat, vec3 n_s, vec3 wo, out vec3 wi, out float pdf_w, out float cos_theta, uint mode,
				  bool forward_facing) {
	wi = vec3(0);
	pdf_w = 0.0;
	cos_theta = 0.0;
	if (dot(n_s, wo) <= 0.0) {
		return vec3(0);
	}

	vec3 eta_scale;
	if (!refract(n_s, wo, forward_facing, mat.ior, mode, wi, eta_scale)) {
		// Total internal reflection
		wi = reflect(-wo, n_s);
		cos_theta = dot(n_s, wi);
		pdf_w = 1.0;
		return mat.albedo / cos_theta;
	}

	cos_theta = dot(n_s, wi);
	pdf_w = 1.0;
	return mat.albedo * eta_scale / abs(cos_theta);
}

vec3 eval_glass(out float pdf_w, out float pdf_rev_w) {
	pdf_w = 0.0;
	pdf_rev_w = 0.0;
	return vec3(0);
}

float eval_glass_pdf() { return 0.0; }
#endif
