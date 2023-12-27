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

		if ((!has_reflection && !has_transmission) || (has_transmission && F == 0.0)) {
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
		wi = reflect(-wo, h);
		// Make sure the reflection lies in the same hemisphere
		if (wo.z * wi.z < 0) {
			return vec3(0);
		}
		// Multiply by reflection jacobian determinant + lobe PDF
		pdf_w = pdf_w * (pr / (pr + pt)) / (4.0 * abs(dot(wo, h)));
		f = vec3(0.25 * D * F * G_GGX_correlated_isotropic(alpha, wo, wi) / (wi.z * wo.z));
	} else {
		float possibly_modified_eta;
		bool tir = !refract(vec3(0, 0, 1), wo, forward_facing, mat.ior, mode, wi, f, possibly_modified_eta);
		if (wo.z * wi.z > 0 || wi.z > 0 || tir) {
			return vec3(0);
		}
		float jacobian_denom = dot(wi, h) + dot(wo, h) * possibly_modified_eta;
		jacobian_denom = jacobian_denom * jacobian_denom;
		float jacobian = abs(dot(wi, h)) / jacobian_denom;
		pdf_w = pdf_w * (pr / (pr + pt)) * jacobian;

		f = f * (1.0 - F) * D * G_GGX_correlated_isotropic(alpha, wo, wi) *
			abs(dot(wi, h) * dot(wo, h) / (wi.z * wo.z * jacobian_denom));
	}
	return f;
}

vec3 eval_dielectric(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w) {
	pdf_w = 0.0;
	pdf_rev_w = 0.0;
	if (wo.z <= 0.0) {
		return vec3(0);
	}
	float alpha = mat.roughness * mat.roughness;
	if (alpha == 0 || mat.ior == 1) {
		return vec3(0);
	}
	return vec3(0);
}

float eval_dielectric_pdf(Material mat, vec3 wo, vec3 wi) {
	float alpha = mat.roughness * mat.roughness;
	if (alpha == 0 || mat.ior == 1) {
		return 0.0;
	}
	return 0.0;
}