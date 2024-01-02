#ifndef PRINCIPLED_GLSL
#define PRINCIPLED_GLSL
#include "microfacet_commons.glsl"

// Disney BSDF: https://blog.selfshadow.com/publications/s2015-shading-course/burley/s2015_pbs_disney_bsdf_notes.pdf
// https://github.dev/schuttejoe/Selas/blob/dev/Source/Core/Shading/Disney.cpp
// https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/
// https://schuttejoe.github.io/post/disneybsdf/
// https://github.dev/mitsuba-renderer/mitsuba3
// Nvidia Falcor

void initialize_sampling_pdfs(const Material mat, out float p_spec, out float p_diff, out float p_clearcloat,
							  out float p_spec_trans) {
	float metallic_brdf = mat.metallic * (1.0 - mat.spec_trans);
	float specular_brdf = mat.spec_trans;
	float dielectric_brdf = (1.0 - mat.spec_trans) * (1.0 - mat.metallic);

	float specular_weight = metallic_brdf + dielectric_brdf;
	float transmission_weight = specular_brdf;
	float diffuse_weight = dielectric_brdf;
	float clearcoat_weight = clamp(mat.clearcoat, 0.0, 1.0);

	float norm = 1.0 / (specular_weight + transmission_weight + diffuse_weight + clearcoat_weight);

	p_spec = specular_weight * norm;
	p_spec_trans = transmission_weight * norm;
	p_diff = diffuse_weight * norm;
	p_clearcloat = clearcoat_weight * norm;
}

vec3 calc_tint(vec3 albedo) {
	float lum = luminance(albedo);
	return lum > 0 ? albedo / lum : vec3(1);
}

float eta_to_schlick_R0(float eta) {
	float val = (eta - 1.0) / (eta + 1.0);
	return val * val;
}

// Always used in BRDF context
vec3 disney_fresnel(const Material mat, vec3 wo, vec3 h, vec3 wi, float eta) {
	// Always equivalent to dot(wi, h)
	float wo_dot_h = dot(wo, h);
	vec3 tint = calc_tint(mat.albedo);

	vec3 R0 = eta_to_schlick_R0(eta) * mix(vec3(1.0), calc_tint(mat.albedo), mat.specular_tint);

	R0 = mix(R0, mat.albedo, mat.metallic);

	// Eta already accounts for which face it corresponds to. Therefore forward_facing is set to true
	float fr_dielectric = fresnel_dielectric(wo_dot_h, eta, true);
	vec3 fr_metallic = fresnel_schlick(R0, vec3(1), wo_dot_h);

	return mix(vec3(fr_dielectric), fr_metallic, mat.metallic);
}

vec3 sample_disney_brdf(const Material mat, const vec3 wo, inout vec3 wi, inout float pdf_w, inout float cos_theta,
						const vec2 xi, float eta) {
	bool has_reflection = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_REFLECTION);
	if ((!has_reflection)) {
		return vec3(0);
	}
	float D;
	float alpha = mat.roughness * mat.roughness;
	vec3 h = sample_ggx_vndf_isotropic(vec2(alpha), wo, xi, pdf_w, D);

	wi = reflect(-wo, h);
	// Make sure the reflection lies in the same hemisphere
	if (wo.z * wi.z < 0) {
		return vec3(0);
	}
	vec3 F = disney_fresnel(mat, wo, h, wi, eta);
	pdf_w /= (4.0 * dot(wo, h));
	cos_theta = wi.z;
	return 0.25 * D * F * G_GGX_correlated_isotropic(alpha, wo, wi) / (wi.z * wo.z);
}

vec3 sample_principled(const Material mat, const vec3 wo, out vec3 wi, const uint mode, const bool forward_facing,
					   out float pdf_w, out float cos_theta, vec2 xi) {
	wi = vec3(0);
	pdf_w = 0.0;
	cos_theta = 0.0;

	if (wo.z <= 0.0) {
		return vec3(0);
	}
	float p_spec, p_diff, p_clearcoat, p_spec_trans;
	initialize_sampling_pdfs(mat, p_spec, p_diff, p_clearcoat, p_spec_trans);

	float alpha = mat.roughness * mat.roughness;

	float eta = forward_facing ? mat.ior : 1.0 / mat.ior;

	vec3 f = vec3(0);
	if (xi.x < p_spec) {
		xi.x /= p_spec;
		sample_disney_brdf(mat, wo, wi, pdf_w, cos_theta, xi, eta);
	} else if (xi.x > p_spec && xi.x <= (p_spec + p_clearcoat)) {
		xi.x /= (p_spec + p_clearcoat);
	} else if (xi.x > (p_spec + p_clearcoat) && xi.x <= (p_spec + p_clearcoat + p_diff)) {
		xi.x /= (p_spec + p_clearcoat + p_diff);
	} else if (p_spec_trans >= 0.0) {
	}
	return f;
}

vec3 eval_principled(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w, bool forward_facing,
					 uint mode, bool eval_reverse_pdf) {
	return vec3(0);
}

float eval_principled_pdf(Material mat, vec3 wo, vec3 wi, bool forward_facing) {
	return 0;
}

#endif