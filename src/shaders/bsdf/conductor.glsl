#ifndef CONDUCTOR_GLSL
#define CONDUCTOR_GLSL
#include "microfacet_commons.glsl"

// Conductor BRDF assumes no refraction
vec3 sample_conductor(const Material mat, const vec3 wo, out vec3 wi, out float pdf_w, out float cos_theta,
					  const vec2 xi) {
	wi = vec3(0);
	pdf_w = 0.0;
	cos_theta = 0.0;

	if (wo.z <= 0.0) {
		return vec3(0);
	}

	float alpha = mat.roughness * mat.roughness;

	bool has_reflection = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_REFLECTION);

	if ((!has_reflection)) {
		return vec3(0);
	}
	// Perfect reflection/transmission
	if (bsdf_is_delta(alpha)) {
		wi = vec3(-wo.x, -wo.y, wo.z);
		pdf_w = 1.0;
		cos_theta = wi.z;
		vec3 F = fresnel_conductor(cos_theta, mat.albedo, mat.eta);
		return F / cos_theta;
	}
	float D;
	vec3 h = sample_ggx_vndf_isotropic(vec2(alpha), wo, xi, pdf_w, D);
	vec3 F = fresnel_conductor(dot(wo, h), mat.albedo, mat.eta);

	wi = reflect(-wo, h);
	// Make sure the reflection lies in the same hemisphere
	if (wo.z * wi.z < 0) {
		return vec3(0);
	}
	pdf_w /= (4.0 * dot(wo, h));
	cos_theta = wi.z;
	return 0.25 * D * F * G_GGX_correlated_isotropic(alpha, wo, wi) / (wi.z * wo.z);
}

vec3 eval_conductor(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w, bool eval_reverse_pdf) {
	pdf_w = 0;
	pdf_rev_w = 0;
	float alpha = mat.roughness * mat.roughness;
	if (bsdf_is_delta(alpha)) {
		return vec3(0);
	}
	if (wo.z * wi.z < 0) {
		return vec3(0);
	}
	if (wo.z == 0 || wi.z == 0) {
		return vec3(0);
	}
	vec3 h = normalize(wo + wo);
	// Make sure h is oriented towards the normal
	h *= float(sign(h.z));

	float jacobian = 1.0 / (4.0 * dot(wo, h));

	float D;
	pdf_w = eval_vndf_pdf_isotropic(alpha, wo, h, D) / jacobian;
	if (eval_reverse_pdf) {
		pdf_rev_w = eval_vndf_pdf_isotropic(alpha, wi, h) / jacobian;
	}

	vec3 F = fresnel_conductor(dot(wo, h), mat.albedo, mat.eta);
	return 0.25 * D * F * G_GGX_correlated_isotropic(alpha, wo, wi) / (wi.z * wo.z);
}

float eval_conductor_pdf(Material mat, vec3 wo, vec3 wi) {
	float alpha = mat.roughness * mat.roughness;
	if (bsdf_is_delta(alpha)) {
		return 0.0;
	}
	if (wo.z * wi.z < 0) {
		return 0.0;
	}
	if (wo.z == 0 || wi.z == 0) {
		return 0.0;
	}
	vec3 h = normalize(wo + wo);
	// Make sure h is oriented towards the normal
	h *= float(sign(h.z));

	return eval_vndf_pdf_isotropic(alpha, wo, h) / (4.0 * dot(wo, h));
}
#endif