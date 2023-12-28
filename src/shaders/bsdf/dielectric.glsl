#include "microfacet_commons.glsl"

vec3 sample_dielectric(const Material mat, const vec3 wo, out vec3 wi, const uint mode, const bool forward_facing,
					   out float pdf_w, out float cos_theta, const vec2 xi) {
	wi = vec3(0);
	pdf_w = 0.0;
	cos_theta = 0.0;

	if (wo.z <= 0.0) {
		return vec3(0);
	}

	float alpha = mat.roughness * mat.roughness;

	bool has_reflection = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_REFLECTION);
	bool has_transmission = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_TRANSMISSION);
	// Perfect reflection/transmission
	if (mat.ior == 1.0 || bsdf_is_delta(alpha)) {
		float F = fresnel_dielectric(wo.z, mat.ior, forward_facing);

		ASSERT1(F <= 1, "%f\n", F);

		if ((!has_reflection && !has_transmission)) {
			return vec3(0);
		}
		float pr = F;
		float pt = 1.0 - F;

		if (!has_reflection) {
			pr = 0.0;
		}
		if (!has_transmission) {
			pt = 0.0;
		}

		bool is_reflection = ((pr + pt) * xi.x) < pr;
		vec3 f;
		if (is_reflection) {
			wi = vec3(-wo.x, -wo.y, wo.z);
			cos_theta = abs(wi.z);
			f = vec3(F) / (cos_theta);
			pdf_w = pr / (pr + pt);
		} else {
			if (!refract(vec3(0, 0, 1), wo, forward_facing, mat.ior, mode, wi, f)) {
				return vec3(0);
			}
			cos_theta = abs(wi.z);
			f = f * (1.0 - F) / cos_theta;
			pdf_w = pt / (pr + pt);
		}
		return f;
	}
	// Rough reflection/transmission
	if (!has_reflection && !has_transmission) {
		return vec3(0);
	}

	float D;
	vec3 h = sample_ggx_vndf_isotropic(vec2(alpha), wo, xi, pdf_w, D);
	float F = fresnel_dielectric(dot(wo, h), mat.ior, forward_facing);

	float pr = has_reflection ? F : 0.0;
	float pt = has_transmission ? (1.0 - F) : 0.0;

	bool is_reflection = ((pr + pt) * xi.x) < pr;
	vec3 f;
	if (is_reflection) {
		wi = reflect(-wo, h);
		// Make sure the reflection lies in the same hemisphere
		if (wo.z * wi.z < 0) {
			return vec3(0);
		}
		// Multiply by reflection jacobian determinant + lobe PDF
		pdf_w = pdf_w * (pr / (pr + pt)) / (4.0 * abs(dot(wo, h)));
		f = vec3(0.25 * D * F * G_GGX_correlated_isotropic(alpha, wo, wi) / (wi.z * wo.z));
	} else {
		float possibly_modified_inv_eta;
		bool tir = !refract(h, wo, forward_facing, mat.ior, mode, wi, f, possibly_modified_inv_eta);
		if (wo.z * wi.z > 0 || wi.z > 0 || tir) {
			return vec3(0);
		}
		float jacobian_denom = dot(wi, h) + dot(wo, h) * possibly_modified_inv_eta;
		jacobian_denom = jacobian_denom * jacobian_denom;
		float jacobian = abs(dot(wi, h)) / jacobian_denom;
		pdf_w = pdf_w * (pt / (pr + pt)) * jacobian;

		f = f * (1.0 - F) * D * G_GGX_correlated_isotropic(alpha, wo, wi) *
			abs(dot(wi, h) * dot(wo, h) / (wi.z * wo.z * jacobian_denom));
	}
	cos_theta = abs(wi.z);
	return f;
}

vec3 eval_dielectric(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w, bool forward_facing,
					 uint mode, bool eval_reverse_pdf) {
	pdf_w = 0.0;
	pdf_rev_w = 0.0;
	float alpha = mat.roughness * mat.roughness;
	if (alpha == 0 || mat.ior == 1) {
		return vec3(0);
	}
	bool has_reflection = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_REFLECTION);
	bool has_transmission = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_TRANSMISSION);

	if (!has_reflection && !has_transmission) {
		return vec3(0);
	}

	float eta = 1.0;
	bool is_reflection = wi.z * wo.z > 0.0;
	if (!is_reflection) {
		eta = forward_facing ? mat.ior : 1.0 / mat.ior;
	}
	vec3 h = normalize(wo + wi * eta);
	// Make sure h is oriented towards the normal
	h *= float(sign(h.z));
	if (wi.z == 0 || wo.z == 0 || dot(h, h) == 0) {
		return vec3(0);
	}
	if (dot(wi, h) * wi.z < 0 || dot(wo, h) * wo.z < 0) {
		return vec3(0);
	}

	float F = fresnel_dielectric(dot(wo, h), mat.ior, forward_facing);

	float pr = has_reflection ? F : 0.0;
	float pt = has_transmission ? (1.0 - F) : 0.0;

	float D;
	if (eval_reverse_pdf) {
		pdf_rev_w = eval_vndf_pdf_isotropic(alpha, wi, -h, D);
	}
	pdf_w = eval_vndf_pdf_isotropic(alpha, wo, h, D);
	float G = G_GGX_correlated_isotropic(alpha, wo, wi);

	vec3 f;
	if (is_reflection) {
		float jacobian = 1.0 / (4.0 * abs(dot(wo, h)));
		float prob_reflection = pr / (pr + pt);
		pdf_w = pdf_w * jacobian * prob_reflection;

		if (eval_reverse_pdf) {
			float jacobian_reverse = 1.0 / (4.0 * abs(dot(wi, h)));
			pdf_rev_w = pdf_rev_w * jacobian_reverse * prob_reflection;
		}
		f = vec3(0.25 * D * G * F / abs(wo.z * wi.z));
	} else {
		float jacobian_denom = dot(wi, h) + dot(wo, h) / eta;
		jacobian_denom = jacobian_denom * jacobian_denom;

		float jacobian = abs(dot(wi, h)) / jacobian_denom;

		vec3 f = vec3(D * G * (1.0 - F) * abs(dot(wi, h) * dot(wo, h) / (wi.z * wo.z * jacobian_denom)));
		if (mode == 1) {
			f /= (eta * eta);
		}

		float prob_refraction = pt / (pr + pt);
		pdf_w = pdf_w * jacobian * prob_refraction;
		if (eval_reverse_pdf) {
			// Reverse the half normal direction
			h *= -1.0;
			jacobian_denom = eta * dot(wi, h) + dot(wo, h);
			jacobian_denom *= jacobian_denom;
			float jacobian_reverse = abs(dot(wo, h)) / jacobian_denom;
			pdf_rev_w = pdf_rev_w * jacobian_reverse * prob_refraction;
		}
	}

	return f;
}

float eval_dielectric_pdf(Material mat, vec3 wo, vec3 wi, bool forward_facing) {
	float alpha = mat.roughness * mat.roughness;
	if (alpha == 0 || mat.ior == 1) {
		return 0.0;
	}

	bool has_reflection = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_REFLECTION);
	bool has_transmission = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_TRANSMISSION);

	if (!has_reflection && !has_transmission) {
		return 0.0;
	}

	bool is_reflection = wi.z * wo.z > 0;
	float eta = 1.0;
	if (!is_reflection) {
		eta = forward_facing ? mat.ior : 1.0 / mat.ior;
	}
	vec3 h = normalize(wo + wi * eta);
	// Make sure h is oriented towards the normal
	h *= float(sign(h.z));
	if (wi.z == 0 || wo.z == 0 || dot(h, h) == 0) {
		return 0.0;
	}
	if (dot(wi, h) * wi.z < 0 || dot(wo, h) * wo.z < 0) {
		return 0.0;
	}

	float F = fresnel_dielectric(dot(wo, h), mat.ior, forward_facing);
	float pr = has_reflection ? F : 0.0;
	float pt = has_transmission ? (1.0 - F) : 0.0;

	float D;
	float pdf_w = eval_vndf_pdf_isotropic(alpha, wo, h, D);
	if (is_reflection) {
		float jacobian = 1.0 / (4.0 * abs(dot(wo, h)));
		float prob_reflection = pr / (pr + pt);
		pdf_w = pdf_w * jacobian * prob_reflection;

	} else {
		float jacobian_denom = dot(wi, h) + dot(wo, h) / eta;
		jacobian_denom = jacobian_denom * jacobian_denom;

		float jacobian = abs(dot(wi, h)) / jacobian_denom;
		float prob_refraction = pt / (pr + pt);
		pdf_w = pdf_w * jacobian * prob_refraction;
	}

	return pdf_w;
}