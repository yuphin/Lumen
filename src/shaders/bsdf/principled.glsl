#ifndef PRINCIPLED_GLSL
#define PRINCIPLED_GLSL
#include "microfacet_commons.glsl"
#include "diffuse.glsl"
#include "dielectric.glsl"

#define ANISOTROPIC 1

// Resources and references:
// Disney BSDF 2012:
// https://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf
// Disney BSDF 2015:
// https://blog.selfshadow.com/publications/s2015-shading-course/burley/s2015_pbs_disney_bsdf_notes.pdf
// https://github.dev/schuttejoe/Selas/blob/dev/Source/Core/Shading/Disney.cpp
// https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/
// https://schuttejoe.github.io/post/disneybsdf/
// https://github.dev/mitsuba-renderer/mitsuba3
// Nvidia Falcor (StandardBSDF)

vec2 calc_anisotropy(float roughness, float anisotropic) {
	float aspect = sqrt(1.0 - 0.9 * anisotropic);
	float roughness_sqr = roughness * roughness;
	return vec2(max(0.001, roughness_sqr / aspect), max(0.001, roughness_sqr * aspect));
}

float principled_diffuse_weight(const Material mat) { return (1.0 - mat.metallic) * (1.0 - mat.spec_trans); }

float principled_transmission_weight(const Material mat) { return (1.0 - mat.metallic) * mat.spec_trans; }

Material principled_transmission_material(Material mat) {
	mat.bsdf_props = (mat.bsdf_props | BSDF_FLAG_TRANSMISSION) & ~BSDF_FLAG_REFLECTION;
	return mat;
}

float principled_interface_fresnel(const Material mat, float cos_theta, bool forward_facing) {
	return fresnel_dielectric(cos_theta, mat.ior, mat.thin == 1 ? true : forward_facing);
}

void initialize_sampling_probs(const Material mat, float F_dielectric, bool forward_facing, out float p_spec_reflect,
							   out float p_diff, out float p_clearcoat, out float p_spec_trans) {
	float brdf_weight = principled_diffuse_weight(mat);
	float bsdf_weight = principled_transmission_weight(mat);
	forward_facing = mat.thin == 1 ? true : forward_facing;

	// Note: forward_facing == false indicates the ray is inside the material

	// Outside: 1.0 - microfacet transmission -> microfacet reflection
	// Inside: microfacet reflection
	p_spec_reflect = forward_facing ? (1.0 - bsdf_weight * (1.0 - F_dielectric)) : F_dielectric;
	// Outside: microfacet transmission
	// Inside: microfacet transmission
	p_spec_trans = forward_facing ? (bsdf_weight * (1.0 - F_dielectric)) : (1.0 - F_dielectric);

	p_diff = forward_facing ? brdf_weight : 0.0;
	p_clearcoat = forward_facing ? 0.25 * clamp(mat.clearcoat, 0.0, 1.0) : 0.0;

	float norm = 1.0 / (p_spec_reflect + p_spec_trans + p_diff + p_clearcoat);
	p_spec_reflect *= norm;
	p_diff *= norm;
	p_clearcoat *= norm;
	p_spec_trans *= norm;
}

vec3 calc_disney_diffuse_factor(Material mat, vec3 wo, vec3 wi) {
	vec3 h = normalize(wi + wo);

	float f_wi = schlick_w(wi.z);
	float f_wo = schlick_w(wo.z);
	float cos_theta_d = dot(wi, h);

	float ss = 0;

	// Retro-reflection
	float rr = 2.0 * mat.roughness * cos_theta_d * cos_theta_d;
	float f_retro = rr * (f_wi + f_wo + f_wi * f_wo * (rr - 1.0));

	float f_diff = (1.0 - 0.5 * f_wi) * (1.0 - 0.5 * f_wo);
	if (mat.flatness > 0.0) {
		float fss90 = 0.5 * rr;
		float f_ss = mix(1.0, fss90, f_wi) * mix(1.0, fss90, f_wo);
		ss = 1.25 * (f_ss * (1.0 / (wi.z + wo.z) - 0.5) + 0.5);
	}
	float ss_approx_and_diff = mix(f_diff + f_retro, ss, mat.flatness);
	return mat.albedo * ss_approx_and_diff * INV_PI;
}

float clearcoat_alpha(const Material mat) { return mix(0.1, 0.001, mat.clearcoat_gloss); }

float D_GTR1(float alpha, float cos_theta) {
	float alpha_sqr = alpha * alpha;
	return (alpha_sqr - 1.0) /
		   (PI * log(alpha_sqr) * (1.0 + (alpha_sqr - 1.0) * cos_theta * cos_theta));
}

vec3 sample_gtr1(float alpha, vec2 xi, out float D) {
	float alpha_sqr = alpha * alpha;
	float cos_theta = sqrt((1.0 - pow(alpha_sqr, 1.0 - xi.x)) / (1.0 - alpha_sqr));
	float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
	float phi = TWO_PI * xi.y;
	vec3 h = vec3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);
	D = D_GTR1(alpha, cos_theta);
	return h;
}

float clearcoat_pdf_from_half_vector(Material mat, vec3 wo, vec3 h) {
	return D_GTR1(clearcoat_alpha(mat), h.z) * h.z / (4.0 * dot(wo, h));
}

float calc_clearcoat_factor(Material mat, vec3 wo, vec3 wi, vec3 h, out float D) {
	D = D_GTR1(clearcoat_alpha(mat), h.z);
	float F = fresnel_schlick(0.04, 1.0, dot(wi, h));
	const float masking_alpha_sqr = 0.25 * 0.25;
	float G = G1_GGX_isotropic(masking_alpha_sqr, wo.z) * G1_GGX_isotropic(masking_alpha_sqr, wi.z);
	return 0.25 * mat.clearcoat * D * F * G / (4.0 * wo.z * wi.z);
}

// Diffuse response, including retro-reflection and the existing flatness approximation.
vec3 sample_disney_diffuse(Material mat, vec3 wo, out vec3 wi, out float pdf_w, out float cos_theta, vec2 xi) {
	wi = sample_hemisphere(xi);
	cos_theta = wi.z;
	pdf_w = cos_theta * INV_PI;
	if (min(wi.z, wo.z) <= 0.0) {
		return vec3(0);
	}
	return calc_disney_diffuse_factor(mat, wo, wi);
}

vec3 sample_principled_brdf(const Material mat, const vec3 wo, inout vec3 wi, inout float pdf_w, inout float cos_theta,
							const vec2 xi, float eta) {
	bool has_reflection = bsdf_has_property(mat.bsdf_props, BSDF_FLAG_REFLECTION);
	if ((!has_reflection)) {
		return vec3(0);
	}
	float D;
#if ANISOTROPIC
	vec2 alpha = calc_anisotropy(mat.roughness, mat.anisotropy);
#else
	float alpha = mat.roughness * mat.roughness;
#endif

	if (bsdf_is_effectively_delta(alpha)) {
		wi = vec3(-wo.x, -wo.y, wo.z);
		pdf_w = 1.0;
		cos_theta = wi.z;
		vec3 F = disney_fresnel(mat, wo, vec3(0, 0, 1), wi, eta);
		return F / abs(cos_theta);
	}
#if ANISOTROPIC
	vec3 h = sample_ggx_vndf_anisotropic(alpha, wo, xi, pdf_w, D);
#else
	vec3 h = sample_ggx_vndf_isotropic(vec2(alpha), wo, xi, pdf_w, D);
#endif

	wi = reflect(-wo, h);
	// Make sure the reflection lies in the same hemisphere
	if (wo.z * wi.z < 0) {
		return vec3(0);
	}
	vec3 F = disney_fresnel(mat, wo, h, wi, eta);
	pdf_w /= (4.0 * dot(wo, h));
	cos_theta = wi.z;
#if ANISOTROPIC
	return 0.25 * D * F * G_GGX_correlated_anisotropic(alpha, wo, wi) / (wi.z * wo.z);
#else
	return 0.25 * D * F * G_GGX_correlated_isotropic(alpha, wo, wi) / (wi.z * wo.z);
#endif
}

vec3 sample_clearcoat(const Material mat, const vec3 wo, inout vec3 wi, inout float pdf_w, inout float cos_theta,
					  const vec2 xi) {
	float D;
	vec3 h = sample_gtr1(clearcoat_alpha(mat), xi, D);

	wi = reflect(-wo, h);
	if (wi.z <= 0.0) {
		return vec3(0);
	}

	float f_clearcoat = calc_clearcoat_factor(mat, wo, wi, h, D);
	pdf_w = D * h.z / (4.0 * dot(wo, h));
	cos_theta = wi.z;

	return vec3(f_clearcoat);
}

vec3 eval_clearcoat(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w) {
	pdf_w = 0;
	pdf_rev_w = 0;
	if (min(wo.z, wi.z) <= 0.0) {
		return vec3(0);
	}
	vec3 h = normalize(wo + wi);
	float D;
	float f_clearcoat = calc_clearcoat_factor(mat, wo, wi, h, D);
	pdf_w = D * h.z / (4.0 * dot(wo, h));
	pdf_rev_w = D * h.z / (4.0 * dot(wi, h));
	return vec3(f_clearcoat);
}
float eval_clearcoat_pdf(Material mat, vec3 wo, vec3 wi) {
	if (min(wo.z, wi.z) <= 0.0) {
		return 0.0;
	}
	vec3 h = normalize(wo + wi);
	return clearcoat_pdf_from_half_vector(mat, wo, h);
}

vec3 eval_principled_brdf(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w, bool forward_facing,
						  uint mode, bool eval_reverse_pdf) {
	pdf_w = 0;
	pdf_rev_w = 0;

#if ANISOTROPIC
	vec2 alpha = calc_anisotropy(mat.roughness, mat.anisotropy);
#else
	float alpha = mat.roughness * mat.roughness;
#endif
	if (bsdf_is_effectively_delta(alpha)) {
		return vec3(0);
	}
	if (wo.z * wi.z < 0) {
		return vec3(0);
	}
	if (wo.z == 0 || wi.z == 0) {
		return vec3(0);
	}
	vec3 h = normalize(wo + wi);
	float jacobian = 1.0 / (4.0 * dot(wo, h));
	float D;
#if ANISOTROPIC
	pdf_w = eval_vndf_pdf_anisotropic(alpha, wo, h, D) * jacobian;
#else
	pdf_w = eval_vndf_pdf_isotropic(alpha, wo, h, D) * jacobian;
#endif
	float eta = forward_facing ? mat.ior : 1.0 / mat.ior;
	if (eval_reverse_pdf) {
#if ANISOTROPIC
		pdf_rev_w = eval_vndf_pdf_anisotropic(alpha, wi, h) * jacobian;
#else
		pdf_rev_w = eval_vndf_pdf_isotropic(alpha, wi, h) * jacobian;
#endif
	}
	vec3 F = disney_fresnel(mat, wo, h, wi, eta);
#if ANISOTROPIC
	return 0.25 * D * F * G_GGX_correlated_anisotropic(alpha, wo, wi) / (wi.z * wo.z);
#else
	return 0.25 * D * F * G_GGX_correlated_isotropic(alpha, wo, wi) / (wi.z * wo.z);
#endif
}

float eval_principled_brdf_pdf(Material mat, vec3 wo, vec3 wi) {
#if ANISOTROPIC
	vec2 alpha = calc_anisotropy(mat.roughness, mat.anisotropy);
#else
	float alpha = mat.roughness * mat.roughness;
#endif

	if (bsdf_is_effectively_delta(alpha)) {
		return 0.0;
	}

	if (wo.z * wi.z < 0) {
		return 0.0;
	}
	if (wo.z == 0 || wi.z == 0) {
		return 0.0;
	}

	vec3 h = normalize(wo + wi);
	// Make sure h is oriented towards the normal
	h *= float(sign(h.z));
#if ANISOTROPIC
	return eval_vndf_pdf_anisotropic(alpha, wo, h) / (4.0 * dot(wo, h));
#else
	return eval_vndf_pdf_isotropic(alpha, wo, h) / (4.0 * dot(wo, h));
#endif
}

vec3 eval_principled(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w, bool forward_facing,
					 uint mode, bool eval_reverse_pdf);

vec3 sample_principled(const Material mat, const vec3 wo, out vec3 wi, const uint mode, const bool forward_facing,
					   out float pdf_w, out float cos_theta, vec3 xi) {
	wi = vec3(0);
	pdf_w = 0.0;
	cos_theta = 0.0;

	if (wo.z <= 0.0) {
		return vec3(0);
	}
	float p_spec, p_diff, p_clearcoat, p_spec_trans;

	float F = principled_interface_fresnel(mat, wo.z, forward_facing);
	initialize_sampling_probs(mat, F, forward_facing, p_spec, p_diff, p_clearcoat, p_spec_trans);

	float eta = forward_facing ? mat.ior : 1.0 / mat.ior;
	Material transmission_mat = principled_transmission_material(mat);

	vec3 f = vec3(0);
	float p_lobe = 0.0;
	bool sampled_delta = false;
	float clearcoat_end = p_spec + p_clearcoat;
	float diffuse_end = clearcoat_end + p_diff;
	if (xi.z < p_spec) {
		f = sample_principled_brdf(mat, wo, wi, pdf_w, cos_theta, xi.xy, eta);
		p_lobe = p_spec;
		sampled_delta = bsdf_is_effectively_delta(calc_anisotropy(mat.roughness, mat.anisotropy));
	} else if (xi.z < clearcoat_end) {
		f = sample_clearcoat(mat, wo, wi, pdf_w, cos_theta, xi.xy);
		p_lobe = p_clearcoat;
	} else if (xi.z < diffuse_end) {
		f = sample_disney_diffuse(mat, wo, wi, pdf_w, cos_theta, xi.xy);
		p_lobe = p_diff;
	} else {
		float transmission_xi = (xi.z - diffuse_end) / p_spec_trans;
		f = principled_transmission_weight(mat) *
			sample_dielectric(transmission_mat, wo, wi, mode, forward_facing, pdf_w, cos_theta,
							  vec3(xi.xy, transmission_xi));
		p_lobe = p_spec_trans;
		sampled_delta = dielectric_is_delta(transmission_mat, dielectric_alpha(transmission_mat));
	}

	pdf_w *= p_lobe;
	if (sampled_delta || pdf_w == 0.0) {
		return f;
	}

	float unused_reverse_pdf;
	return eval_principled(mat, wo, wi, pdf_w, unused_reverse_pdf, forward_facing, mode, false);
}

vec3 eval_principled(Material mat, vec3 wo, vec3 wi, out float pdf_w, out float pdf_rev_w, bool forward_facing,
					 uint mode, bool eval_reverse_pdf) {
	pdf_w = 0.0;
	pdf_rev_w = 0.0;
	float p_spec, p_diff, p_clearcoat, p_spec_trans;
	float F = principled_interface_fresnel(mat, wo.z, forward_facing);
	initialize_sampling_probs(mat, F, forward_facing, p_spec, p_diff, p_clearcoat, p_spec_trans);
	float p_spec_rev = 0.0;
	float p_diff_rev = 0.0;
	float p_clearcoat_rev = 0.0;
	float p_spec_trans_rev = 0.0;
	if (eval_reverse_pdf) {
		bool is_reflection = wo.z * wi.z > 0.0;
		vec3 reverse_wo = is_reflection ? wi : -wi;
		bool reverse_facing = is_reflection ? forward_facing : !forward_facing;
		float F_rev = principled_interface_fresnel(mat, reverse_wo.z, reverse_facing);
		initialize_sampling_probs(mat, F_rev, reverse_facing, p_spec_rev, p_diff_rev, p_clearcoat_rev,
								  p_spec_trans_rev);
	}

	vec3 f = vec3(0);
	float pdf = 0;
	float pdf_rev = 0;

	float brdf_weight = principled_diffuse_weight(mat);
	float bsdf_weight = principled_transmission_weight(mat);
	if (p_spec > 0) {
		f += eval_principled_brdf(mat, wo, wi, pdf, pdf_rev, forward_facing, mode, eval_reverse_pdf);
		pdf_w += p_spec * pdf;
		pdf_rev_w += p_spec_rev * pdf_rev;
	}
	bool upper_hemisphere = min(wi.z, wo.z) > 0;
	if (upper_hemisphere) {
		if (p_diff > 0) {
			pdf_w += p_diff * eval_disney_diffuse_pdf(wo, wi);
			pdf_rev_w += p_diff_rev * eval_disney_diffuse_pdf(wi, wo);
			f += brdf_weight * calc_disney_diffuse_factor(mat, wo, wi);
		}
		if (p_clearcoat > 0) {
			f += eval_clearcoat(mat, wo, wi, pdf, pdf_rev);
			pdf_w += p_clearcoat * pdf;
			pdf_rev_w += p_clearcoat_rev * pdf_rev;
		}
	}

	if (p_spec_trans > 0) {
		Material transmission_mat = principled_transmission_material(mat);
		f += bsdf_weight *
			 eval_dielectric(transmission_mat, wo, wi, pdf, pdf_rev, forward_facing, mode, eval_reverse_pdf);
		pdf_w += p_spec_trans * pdf;
		pdf_rev_w += p_spec_trans_rev * pdf_rev;
	}
	return f;
}

float eval_principled_pdf(Material mat, vec3 wo, vec3 wi, bool forward_facing) {
	float pdf = 0.0;
	float p_spec, p_diff, p_clearcoat, p_spec_trans;
	float F = principled_interface_fresnel(mat, wo.z, forward_facing);
	initialize_sampling_probs(mat, F, forward_facing, p_spec, p_diff, p_clearcoat, p_spec_trans);
	if (p_spec > 0) {
		pdf += p_spec * eval_principled_brdf_pdf(mat, wo, wi);
	}
	bool upper = min(wi.z, wo.z) > 0;
	if (upper) {
		if (p_diff > 0) {
			pdf += p_diff * eval_disney_diffuse_pdf(wo, wi);
		}
		if (p_clearcoat > 0) {
			pdf += p_clearcoat * eval_clearcoat_pdf(mat, wo, wi);
		}
	}
	if (p_spec_trans > 0) {
		pdf += p_spec_trans *
			   eval_dielectric_pdf(principled_transmission_material(mat), wo, wi, forward_facing);
	}
	return pdf;
}

#endif
