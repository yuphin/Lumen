
#include "sampling_commons.glsl"

vec3 sample_lambertian(Material mat, vec3 wo, out vec3 wi, out float pdf_w, out float cos_theta,
							   vec2 xi) {
	wi = sample_hemisphere(xi);
	cos_theta = wi.z;
	pdf_w = cos_theta * INV_PI;
	if (min(wi.z, wo.z) <= 0.0) {
		return vec3(0);
	}
    return mat.albedo * INV_PI;
}

vec3 eval_lambertian(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w) {
    if(min(wi.z, wo.z) <= 0.0) {
        pdf_w = 0;
        pdf_rev_w = 0;
        return vec3(0);
    }
    pdf_w = wi.z * INV_PI;
    pdf_rev_w = pdf_w;
    return mat.albedo * INV_PI;
}


float eval_lambertian_pdf(vec3 wo, vec3 wi) {
    if(min(wi.z, wo.z) <= 0.0) { 
        return 0.0;
    }
    return wi.z * INV_PI;
}

vec3 sample_diffuse(Material mat, vec3 wo, out vec3 wi, out float pdf_w, out float cos_theta,
					vec2 xi) {
	return sample_lambertian(mat,wo, wi, pdf_w, cos_theta, xi);
}


vec3 eval_diffuse(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w) {
  return eval_lambertian(mat, wo, wi, pdf_w, pdf_rev_w);
}

float eval_diffuse_pdf(vec3 wo, vec3 wi) {
  return eval_lambertian_pdf(wo, wi);
}

