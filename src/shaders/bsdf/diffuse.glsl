#ifndef DIFFUSE_GLSL
#define DIFFUSE_GLSL
#include "sampling_commons.glsl"

#define DIFFUSE_BSDF_MODE_LAMBERTIAN 0
#define DIFFUSE_BSDF_MODE_DISNEY 1
#define DIFFUSE_BSDF_MODE_FROSTBITE 2

#ifndef DIFFUSE_BSDF_MODE
#define DIFFUSE_BSDF_MODE DIFFUSE_BSDF_MODE_LAMBERTIAN
#endif

vec3 sample_lambertian(Material mat, vec3 wo, out vec3 wi, out float pdf_w, out float cos_theta, vec2 xi) {
	wi = sample_hemisphere(xi);
	cos_theta = wi.z;
	pdf_w = cos_theta * INV_PI;
	if (min(wi.z, wo.z) <= 0.0) {
		return vec3(0);
	}
	return mat.albedo * INV_PI;
}


float frostbite_fresnel(vec3 wi, vec3 wo, float roughness) {
	vec3 h = normalize(wi + wo);
	float wo_dot_h = dot(wo, h);
	float energy_bias = mix(0.f, 0.5f, roughness);
	float energy_factor = mix(1.f, 1.f / 1.51f, roughness);
	float fd90 = energy_bias + 2.f * wo_dot_h * wo_dot_h * roughness;
	float fd0 = 1.f;
	float f_wi = fresnel_schlick(fd0, fd90, wi.z);
	float f_wo = fresnel_schlick(fd0, fd90, wo.z);
	return f_wi * f_wo * energy_factor;
}

vec3 sample_disney_diffuse_bare(Material mat, vec3 wo, out vec3 wi, out float pdf_w, out float cos_theta, vec2 xi) {
	wi = sample_hemisphere(xi);
	cos_theta = wi.z;
	pdf_w = cos_theta * INV_PI;
	if (min(wi.z, wo.z) <= 0.0) {
		return vec3(0);
	}

	return mat.albedo * disney_fresnel(wi, wo, mat.roughness) * INV_PI;
}

vec3 sample_frosbite_diffuse(Material mat, vec3 wo, out vec3 wi, out float pdf_w, out float cos_theta, vec2 xi) {
	wi = sample_hemisphere(xi);
	cos_theta = wi.z;
	pdf_w = cos_theta * INV_PI;
	if (min(wi.z, wo.z) <= 0.0) {
		return vec3(0);
	}

	return mat.albedo * frostbite_fresnel(wi, wo, mat.roughness) * INV_PI;
}

vec3 eval_lambertian_diffuse(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w) {
	if (min(wi.z, wo.z) <= 0.0) {
		return vec3(0);
	}
	pdf_w = wi.z * INV_PI;
	pdf_rev_w = pdf_w;
	return mat.albedo * INV_PI;
}

vec3 eval_disney_diffuse(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w) {
	if (min(wi.z, wo.z) <= 0.0) {
		return vec3(0);
	}
	pdf_w = wi.z * INV_PI;
	pdf_rev_w = pdf_w;
	return mat.albedo * disney_fresnel(wi, wo, mat.roughness) * INV_PI;
}

vec3 eval_frostbite_diffuse(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w) {
	if (min(wi.z, wo.z) <= 0.0) {
		return vec3(0);
	}
	pdf_w = wi.z * INV_PI;
	pdf_rev_w = pdf_w;
	return mat.albedo * frostbite_fresnel(wi, wo, mat.roughness) * INV_PI;
}

float eval_lambertian_diffuse_pdf(vec3 wo, vec3 wi) {
	if (min(wi.z, wo.z) <= 0.0) {
		return 0.0;
	}
	return wi.z * INV_PI;
}

float eval_disney_diffuse_pdf(vec3 wo, vec3 wi) { return eval_lambertian_diffuse_pdf(wo, wi); }
float eval_frostbite_diffuse_pdf(vec3 wo, vec3 wi) { return eval_lambertian_diffuse_pdf(wo, wi); }

vec3 sample_diffuse(Material mat, vec3 wo, out vec3 wi, out float pdf_w, out float cos_theta, vec2 xi) {
	pdf_w = 0;
#if DIFFUSE_BSDF_MODE == DIFFUSE_BSDF_MODE_LAMBERTIAN
	return sample_lambertian(mat, wo, wi, pdf_w, cos_theta, xi);
#elif DIFFUSE_BSDF_MODE == DIFFUSE_BSDF_MODE_DISNEY
	return sample_disney_diffuse_bare(mat, wo, wi, pdf_w, cos_theta, xi);
#else
	return sample_frosbite_diffuse(mat, wo, wi, pdf_w, cos_theta, xi);
#endif
}

vec3 eval_diffuse(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w) {
	pdf_w = 0;
	pdf_rev_w = 0;
#if DIFFUSE_BSDF_MODE == DIFFUSE_BSDF_MODE_LAMBERTIAN
	return eval_lambertian_diffuse(mat, wo, wi, pdf_w, pdf_rev_w);
#elif DIFFUSE_BSDF_MODE == DIFFUSE_BSDF_MODE_DISNEY
	return eval_disney_diffuse(mat, wo, wi, pdf_w, pdf_rev_w);
#else
	return eval_frostbite_diffuse(mat, wo, wi, pdf_w, pdf_rev_w);
#endif
}

float eval_diffuse_pdf(Material mat, vec3 wo, vec3 wi) {
#if DIFFUSE_BSDF_MODE == DIFFUSE_BSDF_MODE_LAMBERTIAN
	return eval_lambertian_diffuse_pdf(wo, wi);
#elif DIFFUSE_BSDF_MODE == DIFFUSE_BSDF_MODE_DISNEY
	return eval_disney_diffuse_pdf(wo, wi);
#else
	return eval_frostbite_diffuse_pdf(wo, wi);
#endif
}
#endif