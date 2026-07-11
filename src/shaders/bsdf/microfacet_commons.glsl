#ifndef MICROFACT_COMMONS_GLSL
#define MICROFACT_COMMONS_GLSL
#include "sampling_commons.glsl"

// Stable form of the Smith GGX masking term. For a unit direction w, let
//
//   c(w)       = abs(w.z) = abs(cos(w))
//   P_alpha(w) = sqrt((alpha.x*w.x)^2 + (alpha.y*w.y)^2 + w.z^2)
//   Lambda(w)  = 0.5 * (P_alpha(w) / c(w) - 1)
//
// Starting from G1(w) = 1 / (1 + Lambda(w)) and the height-correlated form
// G2(wo,wi) = 1 / (1 + Lambda(wo) + Lambda(wi)), algebraic cancellation gives
//
//   G1(w)     = 2*c(w) / (c(w) + P_alpha(w))
//   G2(wo,wi) = 2*c(wo)*c(wi)
//                / (c(wi)*P_alpha(wo) + c(wo)*P_alpha(wi)).
//
// The isotropic case below is the same expression with alpha.x = alpha.y = alpha.
// This avoids evaluating tan(theta)^2 and the intermediate P_alpha(w) / c(w),
// which are singular at grazing even though the final masking term tends to zero.
float G1_GGX_anisotropic(vec3 w, vec2 alpha) {
	if (w.z <= 0.0) {
		return 0.0;
	}

	float projected_length = length(vec3(alpha * w.xy, w.z));
	return 2.0 * w.z / (w.z + projected_length);
}

float G_GGX_correlated_isotropic(float alpha, vec3 wo, vec3 wi) {
	float cos_o = abs(wo.z);
	float cos_i = abs(wi.z);
	if (cos_o == 0.0 || cos_i == 0.0) {
		return 0.0;
	}
	float projected_o = length(vec3(alpha * wo.xy, wo.z));
	float projected_i = length(vec3(alpha * wi.xy, wi.z));
	return 2.0 * cos_o * cos_i / (cos_i * projected_o + cos_o * projected_i);
}

float G_GGX_correlated_anisotropic(vec2 alpha, vec3 wo, vec3 wi) {
	float cos_o = abs(wo.z);
	float cos_i = abs(wi.z);
	if (cos_o == 0.0 || cos_i == 0.0) {
		return 0.0;
	}
	float projected_o = length(vec3(alpha * wo.xy, wo.z));
	float projected_i = length(vec3(alpha * wi.xy, wi.z));
	return 2.0 * cos_o * cos_i / (cos_i * projected_o + cos_o * projected_i);
}

float D_GGX_anisotropic(vec2 alpha, vec3 h) {
	if (h.z <= 0.0 || min(alpha.x, alpha.y) <= 0.0) {
		return 0.0;
	}
	float d = dot(h.xy / alpha, h.xy / alpha) + h.z * h.z;
	return 1.0 / (PI * alpha.x * alpha.y * d * d);
}

// Eq. 19 in https://blog.selfshadow.com/publications/s2012-shading-course/hoffman/s2012_pbs_physics_math_notes.pdf
float D_GGX_isotropic(float alpha_sqr, float cos_theta) {
	if (alpha_sqr <= 0.0 || cos_theta <= 0.0) {
		return 0.0;
	}
	float d = ((cos_theta * alpha_sqr - cos_theta) * cos_theta + 1);
	if (d == 0.0) {
		return 0.0;
	}
	return alpha_sqr / (d * d * PI);
}

// Eq. 34 in https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
float G1_GGX_isotropic(float alpha_sqr, float cos_theta) {
	if (cos_theta <= 0.0) {
		return 0.0;
	}
	float cos_sqr = cos_theta * cos_theta;
	return 2.0 * cos_theta / (cos_theta + sqrt(alpha_sqr + (1.0 - alpha_sqr) * cos_sqr));
}

// pdf = G1(wo) * D(h) * max(0,dot(wo,h)) / wo.z
float eval_vndf_pdf_isotropic(float alpha, vec3 wo, vec3 h, out float D) {
	D = 0.0;
	if (alpha <= 0.0 || wo.z <= 0.0 || h.z <= 0.0) {
		return 0.0;
	}
	float alpha_sqr = alpha * alpha;
	float G1 = G1_GGX_isotropic(alpha_sqr, wo.z);
	D = D_GGX_isotropic(alpha_sqr, h.z);
	float wo_dot_h = dot(wo, h);
	if (wo_dot_h <= 0.0) {
		return 0.0;
	}
	return G1 * D * wo_dot_h / wo.z;
}

float eval_vndf_pdf_isotropic(float alpha, vec3 wo, vec3 h) {
	float unused_D;
	return eval_vndf_pdf_isotropic(alpha, wo, h, unused_D);
	
}

float eval_vndf_pdf_anisotropic(vec2 alpha, vec3 wo, vec3 h, out float D) {
	D = 0.0;
	if (min(alpha.x, alpha.y) <= 0.0 || wo.z <= 0.0 || h.z <= 0.0) {
		return 0.0;
	}
	float G1 = G1_GGX_anisotropic(wo, alpha);
	D = D_GGX_anisotropic(alpha, h);
	float wo_dot_h = dot(wo, h);
	if (wo_dot_h <= 0.0) {
		return 0.0;
	}
	return G1 * D * wo_dot_h / wo.z;
}

float eval_vndf_pdf_anisotropic(vec2 alpha, vec3 wo, vec3 h)  {
	float unused_D;
	return eval_vndf_pdf_anisotropic(alpha, wo, h, unused_D);
}

vec3 sample_ggx_vndf_common(vec2 alpha, vec3 wo, vec2 xi) {
	vec3 wo_hemisphere = normalize(vec3(alpha.x * wo.x, alpha.y * wo.y, wo.z));

#if 1 // Source: "Sampling Visible GGX Normals with Spherical Caps" by Dupuy & Benyoub
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
	// Do we need this?
	n_h = normalize(n_h);
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
