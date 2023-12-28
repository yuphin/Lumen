#ifndef MICROFACT_COMMONS_GLSL
#define SMICROFACT_COMMONS_GLSL
#include "sampling_commons.glsl"

float Smith_lambda_isotropic(float alpha_sqr, float cos_theta) {
	if (cos_theta == 0) {
		return 0;
	}
	float cos_sqr = cos_theta * cos_theta;
	float tan_sqr = max(1.0 - cos_sqr, 0) / cos_sqr;
	return 0.5 * (sqrt(1.0 + alpha_sqr * tan_sqr) - 1);
}
float Smith_lambda_anisotropic(vec3 w, vec2 alpha) {
	float cos_sqr = w.z * w.z;
	float sin_sqr = max(1.0 - cos_sqr, 0);
	float tan_sqr = sin_sqr / cos_sqr;
	if (isinf(tan_sqr)) {
		return 0.0;
	}
	vec2 phi_sqr = sin_sqr == 0.0 ? vec2(1.0, 0.0) : clamp(vec2(w.x * w.x, w.y * w.y), 0.0, 1.0) / sin_sqr;
	float alpha_sqr = dot(phi_sqr, alpha * alpha);
	return 0.5 * (sqrt(1.0 + alpha_sqr * tan_sqr) - 1);
}

float G1_GGX_anisotropic(vec3 w, vec2 alpha) { return 1.0 / (1.0 + Smith_lambda_anisotropic(w, alpha)); }

float G_GGX_correlated_isotropic(float alpha, vec3 wo, vec3 wi) {
	float alpha_sqr = alpha * alpha;
	return 1.0 / (1.0 + Smith_lambda_isotropic(alpha_sqr, wo.z) + Smith_lambda_isotropic(alpha_sqr, wi.z));
}

float G_GGX_correlated_anisotropic(vec2 alpha, vec3 wo, vec3 wi) {
	return 1.0 / (1.0 + Smith_lambda_anisotropic(wo, alpha) + Smith_lambda_anisotropic(wi, alpha));
}

float D_GGX_anisotropic(vec2 alpha, vec3 h) {
	float cos_sqr = h.z * h.z;
	float sin_sqr = max(1.0f - cos_sqr, 0.0f);
	float tan_sqr = sin_sqr / cos_sqr;
	if (isinf(tan_sqr)) {
		return 0.0f;
	}
	float cos_4 = cos_sqr * cos_sqr;
	if (cos_4 < 1e-16) {
		return 0.0;
	}
	vec2 phi_sqr = sin_sqr == 0.0 ? vec2(1.0, 0.0) : clamp(vec2(h.x * h.x, h.y * h.y), 0.0, 1.0) / sin_sqr;
	vec2 alpha_sqr = phi_sqr / (alpha * alpha);
	float e = tan_sqr * (alpha_sqr.x + alpha_sqr.y);
	return 1.0 / (PI * alpha.x * alpha.y * cos_4 * (1.0 + e) * (1.0 + e));
}

// Eq. 19 in https://blog.selfshadow.com/publications/s2012-shading-course/hoffman/s2012_pbs_physics_math_notes.pdf
float D_GGX_isotropic(float alpha_sqr, float cos_theta) {
	float d = ((cos_theta * alpha_sqr - cos_theta) * cos_theta + 1);
	return alpha_sqr / (d * d * PI);
}

// Eq. 34 in https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
float G1_GGX_isotropic(float alpha_sqr, float cos_theta) {
	float cos_sqr = cos_theta * cos_theta;
	float tan_sqr = max(1.0 - cos_sqr, 0.0) / cos_sqr;
	return 2.0 / (1.0 + sqrt(1.0 + alpha_sqr * tan_sqr));
}

// pdf = G1(wo) * D(h) * max(0,dot(wo,h)) / wo.z
float eval_vndf_pdf_isotropic(float alpha, vec3 wo, vec3 h, out float D) {
	D = 0.0;
	if (wo.z <= 0) {
		return 0.0;
	}
	float alpha_sqr = alpha * alpha;
	float G1 = G1_GGX_isotropic(alpha_sqr, wo.z);
	D = D_GGX_isotropic(alpha_sqr, h.z);
	return G1 * D * max(0.0, dot(wo, h)) / abs(wo.z);
}

float eval_vndf_pdf_anisotropic(vec2 alpha, vec3 wo, vec3 h, out float D) {
	if (wo.z <= 0) {
		return 0.0;
	}
	float G1 = G1_GGX_anisotropic(wo, alpha);
	D = D_GGX_anisotropic(alpha, h);
	return G1 * D * max(0.0, dot(wo, h)) / abs(wo.z);
}

vec3 sample_ggx_vndf_common(vec2 alpha, vec3 wo, vec2 xi) {
	vec3 wo_hemisphere = normalize(vec3(alpha.x * wo.x, alpha.y * wo.y, wo.z));

#if 0  // Source: "Sampling Visible GGX Normals with Spherical Caps" by Dupuy & Benyoub
  	float phi = 2.0 * PI * xi.x;
	float z = ((1.0 - xi.y) * (1.0f + wo_hemisphere.z)) - wo_hemisphere.z;
	float sin_theta = sqrt(clamp(1.0f - z * z, 0.0, 1.0));
	float x = sin_theta * cos(phi);
	float y = sin_theta * sin(phi);

    vec3 n_h = vec3(x, y, z) + wo_hemisphere;
#else

	float lensq = wo_hemisphere.x * wo_hemisphere.x + wo_hemisphere.y * wo_hemisphere.y;
	vec3 T1 = lensq > 0.0 ? vec3(-wo_hemisphere.y, wo_hemisphere.x, 0.0) * inversesqrt(lensq) : vec3(1.0, 0.0, 0.0);
	vec3 T2 = cross(wo_hemisphere, T1);

	float r = sqrt(xi.x);
	float phi = (2.0 * PI) * xi.y;
	float t1 = r * cos(phi);
	float t2 = r * sin(phi);
	float s = 0.5 * (1.0 + wo_hemisphere.z);
	t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
	vec3 n_h = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * wo_hemisphere;
#endif
	return normalize(vec3(alpha.x * n_h.x, alpha.y * n_h.y, max(0.0, n_h.z)));
}

vec3 sample_ggx_vndf_isotropic(vec2 alpha, vec3 wo, vec2 xi, out float pdf, out float D) {
	vec3 h = sample_ggx_vndf_common(alpha, wo, xi);
	pdf = eval_vndf_pdf_isotropic(alpha.x, wo, h, D);
	return h;
}

vec3 sample_ggx_vndf_anisotropic(vec2 alpha, vec3 wo, vec2 xi, out float pdf, out float D) {
	vec3 h = sample_ggx_vndf_common(alpha, wo, xi);
	pdf = eval_vndf_pdf_anisotropic(alpha, wo, h, D);
	return h;
}
#endif