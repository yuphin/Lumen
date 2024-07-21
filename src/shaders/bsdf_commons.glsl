// #define UNIFORM_SAMPLING
// #define CONCENTRIC_DISK_MAPPING
#include "bsdf/diffuse.glsl"
#include "bsdf/mirror.glsl"
#include "bsdf/glass.glsl"
#include "bsdf/dielectric.glsl"
#include "bsdf/conductor.glsl"
#include "bsdf/principled.glsl"

bool is_specular(Material mat) { return (mat.bsdf_props & BSDF_FLAG_SPECULAR) != 0; }

bool is_glossy(Material mat) { return (mat.bsdf_props & BSDF_FLAG_GLOSSY) != 0; }

bool is_diffuse(Material mat) { return (mat.bsdf_props & BSDF_FLAG_DIFFUSE) != 0; }

Material load_material(const uint material_idx, const vec2 uv) {
	Material m = materials.m[material_idx];
	if (m.texture_id > -1) {
		m.albedo *= texture(scene_textures[m.texture_id], uv).xyz;
	}
	return m;
}

bool same_hemisphere(in vec3 wi, in vec3 wo, in vec3 n) { return dot(wi, n) > 0 && dot(wo, n) > 0; }

float bsdf_pdf(const Material mat, const vec3 n_s, vec3 wo, vec3 wi, bool forward_facing) {
	vec3 T, B;
	branchless_onb(n_s, T, B);
	wo = to_local(wo, T, B, n_s);
	wi = to_local(wi, T, B, n_s);
	switch (mat.bsdf_type) {
#ifdef ENABLE_DIFFUSE
		case BSDF_TYPE_DIFFUSE: {
			return eval_diffuse_pdf(mat, wo, wi);
		} break;
#endif
#ifdef ENABLE_MIRROR
		case BSDF_TYPE_MIRROR: {
			return eval_mirror_pdf();
		} break;
#endif
#ifdef ENABLE_GLASS
		case BSDF_TYPE_GLASS: {
			return eval_glass_pdf();
		} break;
#endif
#ifdef ENABLE_DIELECTRIC
		case BSDF_TYPE_DIELECTRIC: {
			return eval_dielectric_pdf(mat, wo, wi, forward_facing);
		} break;
#endif
#ifdef ENABLE_CONDUCTOR
		case BSDF_TYPE_CONDUCTOR: {
			return eval_conductor_pdf(mat, wo, wi);
		} break;
#endif
#ifdef ENABLE_PRINCIPLED
		case BSDF_TYPE_PRINCIPLED: {
			return eval_principled_pdf(mat, wo, wi, forward_facing);
		} break;
#endif
		default:  // Unknown
			break;
	}
	return 0;
}

vec3 sample_bsdf(vec3 n_s, vec3 wo, const Material mat, const uint mode, const bool forward_facing, out vec3 wi,
				 out float pdf_w, out float cos_theta, vec3 rands) {
	vec3 f = vec3(0);
	pdf_w = 0;
	cos_theta = 0;
	wi = vec3(0);

	vec3 T, B;
	branchless_onb(n_s, T, B);
	wo = to_local(wo, T, B, n_s);
	switch (mat.bsdf_type) {
#ifdef ENABLE_DIFFUSE
		case BSDF_TYPE_DIFFUSE: {
			f = sample_diffuse(mat, wo, wi, pdf_w, cos_theta, rands.xy);
		} break;
#endif
#ifdef ENABLE_MIRROR
		case BSDF_TYPE_MIRROR: {
			f = sample_mirror(vec3(0, 0, 1), wo, wi, pdf_w, cos_theta);
		} break;
#endif
#ifdef ENABLE_GLASS
		case BSDF_TYPE_GLASS: {
			f = sample_glass(mat, vec3(0, 0, 1), wo, wi, pdf_w, cos_theta, mode, forward_facing);
		} break;
#endif
#ifdef ENABLE_DIELECTRIC
		case BSDF_TYPE_DIELECTRIC: {
			f = sample_dielectric(mat, wo, wi, mode, forward_facing, pdf_w, cos_theta, rands.xy);
		} break;
#endif
#ifdef ENABLE_CONDUCTOR
		case BSDF_TYPE_CONDUCTOR: {
			f = sample_conductor(mat, wo, wi, pdf_w, cos_theta, rands.xy);
		} break;
#endif
#ifdef ENABLE_PRINCIPLED
		case BSDF_TYPE_PRINCIPLED: {
			f = sample_principled(mat, wo, wi, mode, forward_facing, pdf_w, cos_theta, rands);
		} break;
#endif
		default:  // Unknown
			break;
	}
	wi = to_world(wi, T, B, n_s);
	return f;
}

// Consumes seed 3 times -> seed.w is increemented by 3
vec3 sample_bsdf(vec3 n_s, vec3 wo, const Material mat, const uint mode, const bool forward_facing, out vec3 wi,
				 out float pdf_w, out float cos_theta, inout uvec4 seed) {
	vec3 rands = rand3(seed);
	return sample_bsdf(n_s, wo, mat, mode, forward_facing, wi, pdf_w, cos_theta, rands);
}

vec3 eval_bsdf(const vec3 n_s, vec3 wo, const Material mat, const uint mode, const bool forward_facing, vec3 wi,
			   out float pdf_w, out float pdf_rev_w, bool eval_reverse) {
	pdf_w = 0;
	pdf_rev_w = 0;
	vec3 f = vec3(0);
	vec3 T, B;
	branchless_onb(n_s, T, B);
	wo = to_local(wo, T, B, n_s);
	wi = to_local(wi, T, B, n_s);
	switch (mat.bsdf_type) {
#ifdef ENABLE_DIFFUSE
		case BSDF_TYPE_DIFFUSE: {
			f = eval_diffuse(mat, wo, wi, pdf_w, pdf_rev_w);
		} break;
#endif
#ifdef ENABLE_MIRROR
		case BSDF_TYPE_MIRROR: {
			f = eval_mirror(pdf_w, pdf_rev_w);
		} break;
#endif
#ifdef ENABLE_GLASS
		case BSDF_TYPE_GLASS: {
			f = eval_glass(pdf_w, pdf_rev_w);
		} break;
#endif
#ifdef ENABLE_DIELECTRIC
		case BSDF_TYPE_DIELECTRIC: {
			f = eval_dielectric(mat, wo, wi, pdf_w, pdf_rev_w, forward_facing, mode, eval_reverse);
		} break;
#endif
#ifdef ENABLE_CONDUCTOR
		case BSDF_TYPE_CONDUCTOR: {
			f = eval_conductor(mat, wo, wi, pdf_w, pdf_rev_w, eval_reverse);
		} break;
#endif
#ifdef ENABLE_PRINCIPLED
		case BSDF_TYPE_PRINCIPLED: {
			f = eval_principled(mat, wo, wi, pdf_w, pdf_rev_w, forward_facing, mode, eval_reverse);
		} break;
#endif
		default:  // Unknown
			break;
	}
	return f;
}

vec3 eval_bsdf(const vec3 n_s, vec3 wo, const Material mat, const uint mode, const bool forward_facing, vec3 wi,
			   out float pdf_w, out float pdf_rev_w) {
	return eval_bsdf(n_s, wo, mat, mode, forward_facing, wi, pdf_w, pdf_rev_w, true);
}
vec3 eval_bsdf(const vec3 n_s, const vec3 wo, const Material mat, const uint mode, const bool forward_facing,
			   const vec3 dir, out float pdf_w) {
	float unused_pdf;
	return eval_bsdf(n_s, wo, mat, mode, forward_facing, dir, pdf_w, unused_pdf, false);
}

vec3 eval_bsdf(const Material mat, const vec3 wo, const vec3 wi, const vec3 n_s, uint mode, bool forward_facing) {
	float unused_rev_pdf;
    float unused_pdf;
	return eval_bsdf(n_s, wo, mat, mode, forward_facing, wi, unused_pdf, unused_rev_pdf, false);
}