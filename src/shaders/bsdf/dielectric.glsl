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
	// Perfect reflection/transmission
	if (mat.ior == 1.0 || bsdf_is_delta(alpha)) {
		float F = fresnel_dielectric(wo.z, mat.ior, forward_facing);

		ASSERT1(F <= 1, "%f\n", F);

		bool has_reflection = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_REFLECTION);
		bool has_transmission = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_TRANSMISSION);
		if ((!has_reflection && !has_transmission) || (has_transmission && F == 0.0)) {
			return vec3(0);
		}
		bool is_reflection = xi.x < F;
		float eta = mat.ior;
		vec3 f;
		if (is_reflection) {
			wi = vec3(-wo.x, -wo.y, wo.z);
			cos_theta = abs(wi.z);
			f = vec3(1) / (cos_theta);
			pdf_w = 1.0 / F;
		} else {
			if (!refract(vec3(0, 0, 1), wo, forward_facing, mat.ior, mode, wi, f)) {
				return vec3(0);
			}
			cos_theta = abs(wi.z);
			f /= (cos_theta);
			pdf_w = 1.0 / (1.0 - F);
		}
		return f;
	}

	return vec3(0);
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