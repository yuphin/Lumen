#include "sampling_commons.glsl"
vec3 sample_glass(Material mat, vec3 n_s, vec3 wo, out vec3 wi, out float pdf_w, out float cos_theta, uint mode, bool forward_facing) {
	wi = vec3(0);
	pdf_w = 0.0;
	cos_theta = 0.0;
	vec3 f;
	if(!refract(n_s, wo, forward_facing, mat.ior, mode, wi, f)) {
		return vec3(0);
	}
	cos_theta = dot(n_s, wi);
	pdf_w = 1.;
    f  /= abs(cos_theta);
    return f;
}

vec3 eval_glass(out float pdf_w, out float pdf_rev_w) {
	pdf_w = 0.0;
	pdf_rev_w = 0.0;
	return vec3(0);
}

float eval_glass_pdf() { return 0.0; }