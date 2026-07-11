#ifndef DIELECTRIC_GLSL
#define DIELECTRIC_GLSL
#include "microfacet_commons.glsl"

// Based on Disney's principled BSDF.
// Thin means there's no in-between medium

float modify_thin_roughness(float ior, float roughness) { return clamp((0.65f * ior - 0.35f) * roughness, 0.0, 1.0); }

float dielectric_alpha(const Material mat) {
	float roughness = mat.thin == 1 ? modify_thin_roughness(mat.ior, mat.roughness) : mat.roughness;
	return roughness * roughness;
}

bool dielectric_is_delta(const Material mat, float alpha) {
	return (mat.ior == 1.0 && mat.thin == 0) || bsdf_is_effectively_delta(alpha);
}

vec3 thin_equivalent_reflection(vec3 w) { return vec3(w.xy, -w.z); }

float dielectric_interface_fresnel(const Material mat, float cos_theta, bool forward_facing) {
	// A thin sheet never changes media, so both sides use the exterior Fresnel term.
	return fresnel_dielectric(cos_theta, mat.ior, mat.thin == 1 ? true : forward_facing);
}

bool refraction_jacobian(vec3 wo, vec3 wi, vec3 h, float inv_eta, out float denominator_sqr, out float jacobian) {
	float denominator = dot(wi, h) + dot(wo, h) * inv_eta;
	// The refraction mapping degenerates as a sampled normal approaches critical-angle TIR.
	if (abs(denominator) < 1e-8) {
		return false;
	}
	denominator_sqr = denominator * denominator;
	jacobian = abs(dot(wi, h)) / denominator_sqr;
	return true;
}

bool build_half_vector_for_eval(const Material mat, vec3 wo, vec3 wi, bool forward_facing, out bool is_reflection,
								out vec3 wi_microfacet, out vec3 h, out float eta) {
	is_reflection = wi.z * wo.z > 0.0;
	wi_microfacet = wi;
	h = vec3(0);
	eta = 1.0;

	// wo.z -> under hemisphere
	// wi.z -> grazing the surface
	if (wo.z <= 0.0 || wi.z == 0.0) {
		return false;
	}

	vec3 h_sum;
	if (is_reflection) {
		if (wi.z <= 0.0) {
			return false;
		}
		h_sum = wo + wi;
	} else if (mat.thin == 1) {
		if (wi.z >= 0.0) {
			return false;
		}
		wi_microfacet = thin_equivalent_reflection(wi);
		h_sum = wo + wi_microfacet;
	} else {
		eta = forward_facing ? mat.ior : 1.0 / mat.ior;
		h_sum = wo + wi * eta;
	}

	float h_len_sqr = dot(h_sum, h_sum);
	if (h_len_sqr == 0.0) {
		return false;
	}
	h = h_sum * inversesqrt(h_len_sqr);
	h *= float(sign(h.z));

	if (is_reflection || mat.thin == 1) {
		return dot(wo, h) > 0.0 && dot(wi_microfacet, h) > 0.0;
	}
	return dot(wo, h) * wo.z > 0.0 && dot(wi, h) * wi.z > 0.0;
}

float eval_dielectric_pdf_internal(Material mat, vec3 wo, vec3 wi, bool forward_facing, float alpha) {
	bool is_reflection;
	vec3 wi_microfacet;
	vec3 h;
	float eta;
	if (!build_half_vector_for_eval(mat, wo, wi, forward_facing, is_reflection, wi_microfacet, h, eta)) {
		return 0.0;
	}
	bool has_reflection = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_REFLECTION);
	bool has_transmission = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_TRANSMISSION);
	if ((is_reflection && !has_reflection) || (!is_reflection && !has_transmission)) {
		return 0.0;
	}

	float F = dielectric_interface_fresnel(mat, dot(wo, h), forward_facing);
	float pr = has_reflection ? F : 0.0;
	float pt = has_transmission ? (1.0 - F) : 0.0;
	float probability_sum = pr + pt;

	float pdf_h = eval_vndf_pdf_isotropic(alpha, wo, h);
	if (is_reflection || mat.thin == 1) {
		return pdf_h * ((is_reflection ? pr : pt) / probability_sum) / (4.0 * dot(wo, h));
	}
	float denominator_sqr, jacobian;
	if (!refraction_jacobian(wo, wi, h, 1.0 / eta, denominator_sqr, jacobian)) {
		return 0.0;
	}
	return pdf_h * (pt / probability_sum) * jacobian;
}

vec3 sample_dielectric(const Material mat, const vec3 wo, out vec3 wi, const uint mode, const bool forward_facing,
					   out float pdf_w, out float cos_theta, const vec3 xi) {
	wi = vec3(0);
	pdf_w = 0.0;
	cos_theta = 0.0;

	if (wo.z <= 0.0) {
		return vec3(0);
	}

	bool has_reflection = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_REFLECTION);
	bool has_transmission = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_TRANSMISSION);
	if (!has_reflection && !has_transmission) {
		return vec3(0);
	}

	float alpha = dielectric_alpha(mat);
	if (dielectric_is_delta(mat, alpha)) {
		float F = dielectric_interface_fresnel(mat, wo.z, forward_facing);
		float pr = has_reflection ? F : 0.0;
		float pt = has_transmission ? (1.0 - F) : 0.0;
		float probability_sum = pr + pt;

		if (probability_sum * xi.z < pr) {
			wi = vec3(-wo.x, -wo.y, wo.z);
			cos_theta = wi.z;
			pdf_w = pr / probability_sum;
			return vec3(F) / cos_theta;
		}

		vec3 eta_scale = vec3(1.0);
		vec3 base_color;
		if (mat.thin == 1) {
			wi = -wo;
			base_color = sqrt(mat.albedo);
		} else {
			if (!refract(vec3(0, 0, 1), wo, forward_facing, mat.ior, mode, wi, eta_scale)) {
				return vec3(0);
			}
			base_color = mat.albedo;
		}
		cos_theta = wi.z;
		pdf_w = pt / probability_sum;
		return base_color * eta_scale * (1.0 - F) / abs(cos_theta);
	}

	float pdf_h;
	float D;
	vec3 h = sample_ggx_vndf_isotropic(vec2(alpha), wo, xi.xy, pdf_h, D);
	float wo_dot_h = dot(wo, h);

	float F = dielectric_interface_fresnel(mat, wo_dot_h, forward_facing);
	float pr = has_reflection ? F : 0.0;
	float pt = has_transmission ? (1.0 - F) : 0.0;
	float probability_sum = pr + pt;

	bool is_reflection = probability_sum * xi.z < pr;
	vec3 f;
	if (is_reflection || mat.thin == 1) {
		// Thin transmission samples the reflected lobe and flips it afterwards.
		vec3 wi_reflected = reflect(-wo, h);
		if (wi_reflected.z <= 0.0) {
			return vec3(0);
		}
		// wi_reflected.z > 0 implies wo_dot_h > 0.
		pdf_w = pdf_h * ((is_reflection ? pr : pt) / probability_sum) / (4.0 * wo_dot_h);
		float G = G_GGX_correlated_isotropic(alpha, wo, wi_reflected);
		if (is_reflection) {
			wi = wi_reflected;
			f = vec3(0.25 * D * F * G / (wi.z * wo.z));
		} else {
			wi = thin_equivalent_reflection(wi_reflected);
			f = sqrt(mat.albedo) * (1.0 - F) * D * G / (4.0 * wo.z * wi_reflected.z);
		}
	} else {
		vec3 eta_scale;
		float inv_eta;
		if (!refract(h, wo, forward_facing, mat.ior, mode, wi, eta_scale, inv_eta) || wi.z >= 0.0) {
			wi = vec3(0);
			return vec3(0);
		}
		float denominator_sqr, jacobian;
		if (!refraction_jacobian(wo, wi, h, inv_eta, denominator_sqr, jacobian)) {
			wi = vec3(0);
			return vec3(0);
		}
		pdf_w = pdf_h * (pt / probability_sum) * jacobian;
		f = mat.albedo * eta_scale * (1.0 - F) * D * G_GGX_correlated_isotropic(alpha, wo, wi) *
			abs(dot(wi, h) * wo_dot_h / (wi.z * wo.z * denominator_sqr));
	}

	cos_theta = wi.z;
	return f;
}

vec3 eval_dielectric(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w, bool forward_facing,
					 uint mode, bool eval_reverse_pdf) {
	pdf_w = 0.0;
	pdf_rev_w = 0.0;

	float alpha = dielectric_alpha(mat);
	if (dielectric_is_delta(mat, alpha)) {
		return vec3(0);
	}

	bool is_reflection;
	vec3 wi_microfacet;
	vec3 h;
	float eta;
	if (!build_half_vector_for_eval(mat, wo, wi, forward_facing, is_reflection, wi_microfacet, h, eta)) {
		return vec3(0);
	}
	bool has_reflection = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_REFLECTION);
	bool has_transmission = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_TRANSMISSION);
	if ((is_reflection && !has_reflection) || (!is_reflection && !has_transmission)) {
		return vec3(0);
	}

	float F = dielectric_interface_fresnel(mat, dot(wo, h), forward_facing);
	float D = D_GGX_isotropic(alpha * alpha, h.z);

	pdf_w = eval_dielectric_pdf_internal(mat, wo, wi, forward_facing, alpha);
	if (eval_reverse_pdf) {
		pdf_rev_w = is_reflection ? eval_dielectric_pdf_internal(mat, wi, wo, forward_facing, alpha)
								  : eval_dielectric_pdf_internal(mat, -wi, -wo, !forward_facing, alpha);
	}

	vec3 f;
	if (is_reflection) {
		f = vec3(0.25 * D * G_GGX_correlated_isotropic(alpha, wo, wi) * F / (wo.z * wi.z));
	} else if (mat.thin == 1) {
		f = sqrt(mat.albedo) * (1.0 - F) * D * G_GGX_correlated_isotropic(alpha, wo, wi_microfacet) /
			(4.0 * wo.z * wi_microfacet.z);
	} else {
		float denominator_sqr, jacobian;
		if (!refraction_jacobian(wo, wi, h, 1.0 / eta, denominator_sqr, jacobian)) {
			return vec3(0);
		}
		f = mat.albedo * D * G_GGX_correlated_isotropic(alpha, wo, wi) * (1.0 - F) *
			abs(dot(wi, h) * dot(wo, h) / (wi.z * wo.z * denominator_sqr));
		if (mode == 1) {
			f /= eta * eta;
		}
	}

	return f;
}

float eval_dielectric_pdf(Material mat, vec3 wo, vec3 wi, bool forward_facing) {
	float alpha = dielectric_alpha(mat);
	if (dielectric_is_delta(mat, alpha)) {
		return 0.0;
	}
	return eval_dielectric_pdf_internal(mat, wo, wi, forward_facing, alpha);
}
#endif