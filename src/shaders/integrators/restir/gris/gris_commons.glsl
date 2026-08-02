#include "../../../bda.glsl"
#include "gris_commons.h"
#include "../../../commons.glsl"
#include "../../../surface.glsl"
layout(location = 0) rayPayloadEXT SurfaceHitPayload payload;
layout(location = 1) rayPayloadEXT AnyHitPayload any_hit_payload;
#include "../../../shadow_ray.glsl"
layout(push_constant) uniform _PushConstantRay { PCReSTIRPT pc; };

#define LOG_GRIS 0

const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;
#define RR_MIN_DEPTH 3
uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);

#define RECONNECTION_TYPE_INVALID 0
#define RECONNECTION_TYPE_NEE 1
#define RECONNECTION_TYPE_EMISSIVE_AFTER_RC 2
#define RECONNECTION_TYPE_EMISSIVE 3
#define RECONNECTION_TYPE_NEE_AFTER_RC 4
#define RECONNECTION_TYPE_DEFAULT 5

#define STREAMING_MODE_INDIVIDUAL 0
#define STREAMING_MODE_SPLIT 1

#ifndef STREAMING_MODE
#define STREAMING_MODE STREAMING_MODE_INDIVIDUAL
#endif	// STREAMING_MODE

struct OcclusionData {
	vec3 origin;
	vec3 dir;
	float dir_length;
};

ivec2 get_neighbor_offset(inout uvec4 seed) {
	const float randa = rand(seed) * 2 * PI;
	const float randr = sqrt(rand(seed)) * pc.spatial_radius;
	return ivec2(floor(cos(randa) * randr), floor(sin(randa) * randr));
}

vec3 do_nee(inout uvec4 seed, vec3 pos, Material hit_mat, bool side, vec3 n_s, vec3 n_g, vec3 wo, float d_vm,
			out LightSampleIdentity identity, inout vec3 light_dir_or_pdf, out bool is_directional_light, out vec3 Le,
			out vec3 wi, out float pdf_light_w, int depth) {
	const LightLiSample light_sample = sample_light_Li(rand4(seed), pos, pc.num_lights);
	identity = light_sample.identity;
	Le = light_sample.Li;
	wi = light_sample.wi;
	pdf_light_w = light_sample.pdf_position_w;
	const float wi_len = light_sample.distance;
	if (wi_len <= EPS) {
		return vec3(0);
	}
	const uint light_type = get_light_type(light_sample.flags);
	const vec3 p = offset_ray2(pos, n_g, dot(wi, n_g) < 0.0);
	float light_bsdf_pdf_fwd;
	float light_bsdf_pdf_rev;
	float cos_x_s = max(0, dot(n_s, wi));
	float cos_x_g = abs(dot(n_g, wi));
	vec3 f_light = eval_bsdf(n_s, n_g, wo, hit_mat, TRANSPORT_MODE_FROM_CAMERA, side, wi, light_bsdf_pdf_fwd,
							   light_bsdf_pdf_rev);
	bool visible = !connection_occluded(p, wi, wi_len, 0xFF);
	is_directional_light = light_type == LIGHT_DIRECTIONAL;

	light_dir_or_pdf = vec3(light_sample.pdf_position_a, vec2(0));

	if (visible && pdf_light_w > 0 && cos_x_s > 0 && f_light != vec3(0.0)) {
		float mis_light = is_light_delta(light_sample.flags) ? 0 : light_bsdf_pdf_fwd / pdf_light_w;
		ASSERT(mis_light >= 0);
#ifndef DISABLE_PM_MIS
		const float pdf_dir = light_emission_direction_pdf_w(
			light_sample.identity.light_idx, light_sample.normal, -light_sample.wi);
		float mis_eye = wi_len == 0
						? 0
						: pdf_dir * light_bsdf_pdf_rev * cos_x_g * d_vm /
							  (wi_len * wi_len * light_sample.selection_pmf);
		ASSERT(d_vm >= 0);
		ASSERT(mis_eye >= 0);
#else
		float mis_eye = 0;
#endif	// !DISABLE_PM_MIS
		float mis_weight = 1 / (1 + mis_light + mis_eye);
		ASSERT(!isnan(mis_weight));
		ASSERT(mis_weight >= 0);
		return mis_weight * f_light * abs(cos_x_s) * Le / pdf_light_w;
	}
	return vec3(0);
}

vec2 to_spherical(const vec3 v) {
	float phi = (v.z == 0.0 && v.x == 0.0) ? 0.0 : atan(v.z, v.x);
	float theta = acos(clamp(v.y, -1.0, 1.0));
	return vec2(phi, theta);
}

vec3 from_spherical(const vec2 v) {
	float sin_theta = sin(v.y);
	return vec3(sin_theta * cos(v.x), cos(v.y), sin_theta * sin(v.x));
}

void init_gbuffer(out SurfaceRef gbuffer) { gbuffer = invalid_surface_ref(); }

void init_data(out GrisData data) {
	data.path_flags = 0;
	data.rc_surface = invalid_surface_ref();
}

void init_reservoir(out Reservoir r) {
	init_data(r.data);
	r.M = 0;
	r.W = 0.0;
	r.w_sum = 0.0;
	r.target_pdf = 0.0;
}

bool reservoir_data_valid(in GrisData data) { return data.path_flags != 0; }

bool gbuffer_data_valid(in SurfaceRef gbuffer) { return surface_ref_valid(gbuffer); }

bool update_reservoir(inout uvec4 seed, inout Reservoir r_new, const GrisData data, float target_pdf,
					  float inv_source_pdf) {
	float w_i = target_pdf * inv_source_pdf;
	r_new.w_sum += w_i;
	if (rand(seed) * r_new.w_sum < w_i) {
		r_new.data = data;
		r_new.target_pdf = target_pdf;
		return true;
	}
	return false;
}

bool stream_reservoir(inout uvec4 seed, inout Reservoir r_new, const GrisData data, float target_pdf,
					  float inv_source_pdf) {
	r_new.M++;
	if (target_pdf <= 0.0 || isnan(inv_source_pdf) || inv_source_pdf <= 0.0) {
		return false;
	}
	return update_reservoir(seed, r_new, data, target_pdf, inv_source_pdf);
}

bool combine_reservoir(inout uvec4 seed, inout Reservoir target_reservoir, const Reservoir input_reservoir,
					   float target_pdf, float mis_weight, float jacobian) {
	target_reservoir.M += input_reservoir.M;
	float inv_source_pdf = mis_weight * jacobian * input_reservoir.W;
	if (isnan(inv_source_pdf) || inv_source_pdf == 0.0 || isinf(inv_source_pdf)) {
		return false;
	}
	return update_reservoir(seed, target_reservoir, input_reservoir.data, target_pdf, inv_source_pdf);
}

void calc_reservoir_W(inout Reservoir r) {
	float denom = r.target_pdf * r.M;
	r.W = denom == 0.0 ? 0.0 : r.w_sum / denom;
}

void calc_reservoir_W_with_mis(inout Reservoir r) { r.W = r.target_pdf == 0.0 ? 0.0 : r.w_sum / r.target_pdf; }

float calc_target_pdf(vec3 f) { return luminance(f); }

bool is_rough(in Material mat) {
	// Only check if it's diffuse for now
	return (mat.bsdf_type & BSDF_TYPE_DIFFUSE) != 0 || mat.roughness > 0.25;
}

uint offset(const uint pingpong) { return pingpong * pc.width * pc.height; }

uint pack_path_flags(uint prefix_length, uint postfix_length, uint reconnection_type, bool side) {
	return uint(side) << 13 | (postfix_length & 0x1F) << 8 | (prefix_length & 0x1F) << 3 |
		   uint(reconnection_type & 0x7);
}

void unpack_path_flags(uint packed_data, out uint reconnection_type, out uint prefix_length, out uint postfix_length,
					   out bool side, out bool is_directional_light) {
	reconnection_type = (packed_data & 0x7);
	prefix_length = (packed_data >> 3) & 0x1F;
	postfix_length = (packed_data >> 8) & 0x1F;
	side = ((packed_data >> 13) & 1) == 1;
	is_directional_light = ((packed_data >> 14) & 1) == 1;
}

uint pack_photon_flags(bool side, uint path_length) { return (path_length & 0x1F) << 1 | uint(side); }

void unpack_photon_flags(uint flags, out bool side, out uint path_length) {
	side = (flags & 1) == 1;
	path_length = (flags >> 1) & 0x1F;
}

void set_bounce_flag(inout uint flags, uint depth, bool constraints_satisfied) {
	flags |= (uint(constraints_satisfied) << (16 + depth));
}

bool get_bounce_flag(uint flags, uint depth) { return ((flags >> (16 + depth)) & 1) == 1; }

vec3 get_primary_direction(uvec2 coords) {
	const vec2 uv = vec2(coords + vec2(0.5)) / vec2(gl_LaunchSizeEXT.xy);
	const vec2 d = uv * 2.0 - 1;
	return normalize(sample_camera(d).xyz);
}

vec3 get_prev_primary_direction(uvec2 coords) {
	const vec2 uv = vec2(coords + vec2(0.5)) / vec2(gl_LaunchSizeEXT.xy);
	const vec2 d = uv * 2.0 - 1;
	return normalize(sample_prev_camera(d).xyz);
}

bool advance_paths(in SurfaceData dst_gbuffer, in GrisData data, vec3 dst_wi, float src_jacobian, out float jacobian_out,
				   out vec3 reservoir_contribution, out float jacobian_num, out OcclusionData occlusion_data) {
	// Note: The source reservoir always corresponds to the canonical reservoir because retracing only happens on
	// pairwise mode
	reservoir_contribution = vec3(0);
	jacobian_out = 0;
	jacobian_num = 0;

	occlusion_data.dir_length = EPS;
	occlusion_data.origin = vec3(0);
	occlusion_data.dir = vec3(0);

	float jacobian = 1;
	if (!reservoir_data_valid(data)) {
		return false;
	}
	uvec2 rc_coords = uvec2(data.rc_coords / gl_LaunchSizeEXT.y, data.rc_coords % gl_LaunchSizeEXT.y);
	uvec4 reservoir_seed = init_rng(rc_coords, gl_LaunchSizeEXT.xy, data.seed_helpers.y, 0);

	uint rc_type;
	uint rc_prefix_length;
	uint rc_postfix_length;
	bool rc_side;
	bool is_directional_light;
	unpack_path_flags(data.path_flags, rc_type, rc_prefix_length, rc_postfix_length, rc_side, is_directional_light);

	SurfaceData rc_gbuffer;
	Material rc_hit_mat;

	// Unconditionally initializing here causes corruption when rc_type is NEE
	Light light;
	if (rc_type != RECONNECTION_TYPE_NEE) {
		rc_gbuffer = load_surface(data.rc_surface);
		rc_hit_mat = load_material(rc_gbuffer.material_idx, rc_gbuffer.uv);
	} else {
		light = lights[data.rc_surface.primitive_instance_id.y];
	}

	uint prefix_depth = 0;
	vec3 prefix_throughput = vec3(1);

	uint bounce_flags = 0;

	bool prev_far = true;
	bool dst_far;
	bool dst_rough = true;
	bool prev_rough = dst_rough;

	vec3 org_pos;
	while (true) {
		if (((prefix_depth + rc_postfix_length + 1) > pc.max_depth) || prefix_depth > rc_prefix_length) {
			return false;
		}
		vec3 dst_wo = -dst_wi;
		bool dst_side = face_forward(dst_gbuffer.n_s, dst_gbuffer.n_g, dst_wo);
		org_pos = dst_gbuffer.pos;
		Material dst_hit_mat = load_material(dst_gbuffer.material_idx, dst_gbuffer.uv);

		if (rc_type != RECONNECTION_TYPE_NEE) {
			float rc_wi_len = length(rc_gbuffer.pos - org_pos);
			dst_far = rc_wi_len > pc.min_vertex_distance_ratio * pc.scene_extent;
		} else {
			dst_far = true;
		}

		prev_rough = dst_rough;
		dst_rough = is_rough(dst_hit_mat);

		// A simple way to impose the distance condition without breaking symmetry
		if (rc_type == RECONNECTION_TYPE_NEE && dst_rough &&
			get_bounce_flag(data.path_flags, prefix_depth) != dst_rough) {
			return false;
		}
		set_bounce_flag(bounce_flags, prefix_depth, dst_rough && prev_far);

		if (prefix_depth == rc_prefix_length) {
			dst_far = rc_type == RECONNECTION_TYPE_NEE ? true : dst_far;
			bool constraints_satisfied = dst_rough && dst_far;
			// Reconnection
			if (!constraints_satisfied || (prefix_depth + rc_postfix_length + 1) > pc.max_depth) {
				return false;
			}
#if DEBUG == 1
			ASSERT(data.debug_sampling_seed == reservoir_seed)
#endif

			LightLiSample replayed_light;
			vec3 dst_postfix_wi;
			if (rc_type == RECONNECTION_TYPE_NEE) {
				LightSampleIdentity identity;
				identity.light_idx = data.rc_surface.primitive_instance_id.y;
				identity.primitive_idx = light.prim_mesh_idx;
				identity.triangle_idx = data.rc_surface.primitive_instance_id.x;
				identity.bary = data.rc_surface.barycentrics;
				replayed_light = replay_light_Li(identity, org_pos, pc.num_lights);
				dst_postfix_wi = replayed_light.wi * replayed_light.distance;
			} else {
				dst_postfix_wi = rc_gbuffer.pos - org_pos;
			}

			float wi_len_sqr = dot(dst_postfix_wi, dst_postfix_wi);
			float wi_len = sqrt(wi_len_sqr);
			dst_postfix_wi /= wi_len;
			occlusion_data.origin =
				offset_ray2(org_pos, dst_gbuffer.n_g, dot(dst_postfix_wi, dst_gbuffer.n_g) < 0.0);
			occlusion_data.dir = dst_postfix_wi;
			occlusion_data.dir_length = wi_len;
			set_bounce_flag(bounce_flags, prefix_depth + 1, true);
			if (bounce_flags != (data.path_flags & 0xFFFF0000)) {
				return false;
			}

			float rc_cos_x = dot(dst_gbuffer.n_s, dst_postfix_wi);
			float dst_postfix_pdf;
			float unused_rev_pdf;
			vec3 dst_postfix_f = eval_bsdf(dst_gbuffer.n_s, dst_gbuffer.n_g, dst_wo, dst_hit_mat,
										   TRANSPORT_MODE_FROM_CAMERA, dst_side, dst_postfix_wi,
										   dst_postfix_pdf, unused_rev_pdf, false);
			reservoir_contribution = prefix_throughput * abs(rc_cos_x) * dst_postfix_f;

#if 0
			// This line would be needed because reservoir_contribution being 0 means we may get reservoir_contribution turning into nans down the line
			// However, it's turned off because ReSTIR handles NaN propagation down the line
			if(reservoir_contribution == vec3(0)) {
				return false;
			}
#endif
			if (rc_type == RECONNECTION_TYPE_NEE) {
				// In this case directly re-use the NEE result
				ASSERT(prefix_depth != 0);	// Can't process direct lighting
				uvec2 rc_coords = uvec2(data.rc_coords / gl_LaunchSizeEXT.y, data.rc_coords % gl_LaunchSizeEXT.y);
				uvec4 reconnection_seed = init_rng(rc_coords, gl_LaunchSizeEXT.xy, data.seed_helpers.x, data.rc_seed);
#if DEBUG == 1
				ASSERT(reconnection_seed == data.debug_seed);
#endif
				const float mis_weight = is_light_delta(replayed_light.flags)
										 ? 1.0
										 : 1.0 / (1.0 + dst_postfix_pdf / replayed_light.pdf_position_w);
				reservoir_contribution *= replayed_light.Li * mis_weight / replayed_light.pdf_position_w;
#if LOG_GRIS
				LOG_CLICKED3("NEE: %d - %d = %v3f\n", prefix_depth, (data.path_flags) >> 16, reservoir_contribution);
#endif
				jacobian = 1;
			} else {
				const bool connection_to_nee_vertex = rc_postfix_length == 2;
				float g = abs(dot(dst_postfix_wi, rc_gbuffer.n_g)) / wi_len_sqr;

				ASSERT(rc_type == RECONNECTION_TYPE_DEFAULT || rc_type == RECONNECTION_TYPE_EMISSIVE_AFTER_RC ||
					   rc_type == RECONNECTION_TYPE_NEE_AFTER_RC);
				jacobian_num = dst_postfix_pdf * g;

				bool rc_post_side = face_forward(rc_gbuffer.n_s, rc_gbuffer.n_g, -dst_postfix_wi);

				const vec3 rc_wi_post = from_spherical(data.rc_wi);
				float rc_pdf_post;
				vec3 rc_postfix_f = eval_bsdf(rc_gbuffer.n_s, rc_gbuffer.n_g, -dst_postfix_wi, rc_hit_mat,
										 TRANSPORT_MODE_FROM_CAMERA, rc_post_side, rc_wi_post,
											  rc_pdf_post, unused_rev_pdf, false);

				float mis_weight = 1.0;
				if (rc_type == RECONNECTION_TYPE_EMISSIVE_AFTER_RC) {
					ASSERT(rc_postfix_length == 1);
					mis_weight = 1.0 / (1 + uintBitsToFloat(data.rc_seed) / dst_postfix_pdf);
				} else if (rc_type == RECONNECTION_TYPE_NEE_AFTER_RC) {
					// TODO: Handle directional light
					mis_weight = 1.0 / (1 + rc_pdf_post / uintBitsToFloat(data.rc_seed));
				}

				if (rc_type == RECONNECTION_TYPE_NEE_AFTER_RC) {
					reservoir_contribution *= rc_postfix_f * abs(dot(rc_gbuffer.n_s, rc_wi_post)) /
											  uintBitsToFloat(data.rc_seed);

				} else if (rc_type != RECONNECTION_TYPE_EMISSIVE_AFTER_RC) {
					jacobian_num *= rc_pdf_post;
					reservoir_contribution *= rc_postfix_f * abs(dot(rc_gbuffer.n_s, rc_wi_post)) / rc_pdf_post;
				}
				reservoir_contribution *= data.rc_Li * mis_weight / dst_postfix_pdf;
#if LOG_GRIS
				LOG_CLICKED4("Default: %d - %d - %d - %v3f\n", prefix_depth, rc_postfix_length, rc_type,
							 reservoir_contribution);
#endif
				jacobian = jacobian_num / src_jacobian;
			}
			if (isnan(jacobian) || isinf(jacobian) || jacobian == 0) {
				jacobian_num = 0;
				jacobian_out = 0;
				reservoir_contribution = vec3(0);
				return false;
			}
			jacobian_out = jacobian;
			return true;
		}

		float pdf, cos_theta;
		const vec3 f = sample_bsdf(dst_gbuffer.n_s, dst_gbuffer.n_g, dst_wo, dst_hit_mat,
								   TRANSPORT_MODE_FROM_CAMERA, dst_side,
								   dst_wi, pdf, cos_theta, reservoir_seed);

		if (pdf == 0) {
			return false;
		}

		prefix_throughput *= f * abs(cos_theta) / pdf;

		const vec3 ray_origin = offset_ray(org_pos, dst_gbuffer.n_g, dst_wi);
		traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, ray_origin, tmin, dst_wi, tmax, 0);
		const bool found_isect = surface_ref_valid(payload.surface);
		if (!found_isect) {
			return false;
		}

		vec3 prev_pos = org_pos;
		dst_gbuffer = load_surface(payload.surface);
		prev_far = length(dst_gbuffer.pos - prev_pos) > pc.min_vertex_distance_ratio * pc.scene_extent;
		prefix_depth++;
	}
}

bool retrace_paths(in SurfaceData dst_gbuffer, in GrisData data, vec3 dst_wi, float src_jacobian, out float jacobian_out,
				   out vec3 reservoir_contribution, out float jacobian_num) {
	OcclusionData occlusion_data;
	bool result = advance_paths(dst_gbuffer, data, dst_wi, src_jacobian, jacobian_out, reservoir_contribution,
								jacobian_num, occlusion_data);
	if (pc.enable_occlusion == 1 && occlusion_data.dir_length > EPS) {
		any_hit_payload.hit = 1;
		traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 1, 0, 1,
					occlusion_data.origin, 0, occlusion_data.dir, occlusion_data.dir_length - EPS, 1);
		if (any_hit_payload.hit == 1) {
			jacobian_out = 0;
			reservoir_contribution = vec3(0);
			jacobian_num = 0;
			return false;
		}
	}
	return result;
}

bool retrace_paths(in SurfaceData dst_gbuffer, in GrisData data, vec3 dst_wi, out float jacobian_out,
				   out vec3 reservoir_contribution) {
	float unused_jacobian;
	return retrace_paths(dst_gbuffer, data, dst_wi, data.rc_partial_jacobian, jacobian_out, reservoir_contribution,
						 unused_jacobian);
}

bool retrace_paths_and_evaluate(in SurfaceData dst_gbuffer, in GrisData data, vec3 dst_wi, float src_jacobian,
								out float target_pdf) {
	vec3 reservoir_contribution;
	float jacobian = 0;
	float unused_jacobian_num;
	bool result =
		retrace_paths(dst_gbuffer, data, dst_wi, src_jacobian, jacobian, reservoir_contribution, unused_jacobian_num);
	target_pdf = calc_target_pdf(reservoir_contribution) * jacobian;
	return result;
}

bool retrace_paths_and_evaluate(in SurfaceData dst_gbuffer, in GrisData data, vec3 dst_wi, out float target_pdf) {
	return retrace_paths_and_evaluate(dst_gbuffer, data, dst_wi, data.rc_partial_jacobian, target_pdf);
}

bool process_reservoir(inout uvec4 seed, inout Reservoir reservoir, inout float m_c, in Reservoir canonical_reservoir,
					   in Reservoir source_reservoir, in ReconnectionData data, ivec2 neighbor_coords,
					   float canonical_in_canonical_pdf, uint num_spatial_samples,
					   inout vec3 curr_reservoir_contribution) {
	source_reservoir.M = min(source_reservoir.M, 20);

	bool result = false;

	float neighbor_in_neighbor_pdf = source_reservoir.data.reservoir_contribution;
	float neighbor_in_canonical_pdf = calc_target_pdf(data.reservoir_contribution) * data.jacobian;

	float m_i_num = source_reservoir.M * neighbor_in_neighbor_pdf;
	float m_i_denom = m_i_num + canonical_reservoir.M * neighbor_in_canonical_pdf / num_spatial_samples;
	float m_i = neighbor_in_canonical_pdf <= 0 ? 0 : m_i_num / m_i_denom;

	// if (m_i <= 0.0 || m_i >= 1.0) {
	// 	reservoir.M += source_reservoir.M;
	// 	m_c += 1.0;
	// 	return false;
	// }

	m_c += 1.0;
	float m_c_num = source_reservoir.M * data.target_pdf_in_neighbor;
	float m_c_denom = m_c_num + canonical_reservoir.M * canonical_in_canonical_pdf / num_spatial_samples;
	float m_c_val = m_c_denom == 0.0 ? 0.0 : m_c_num / m_c_denom;
	if (m_c_val > 0) {
		m_c -= m_c_val;
	}
	// if (m_c_val <= 0.0) {
	// 	reservoir.M += source_reservoir.M;
	// 	return false;
	// }

	// ASSERT1(m_c_val > -1e-3 && m_c_val <= 1.001, "m_c_val <= 1.0 : %f\n", m_c_val);
	// ASSERT1(m_i > -1e-3 && m_i <= 1.001, "m_i <= 1.0 : %f\n", m_i);

	bool accepted = combine_reservoir(seed, reservoir, source_reservoir, calc_target_pdf(data.reservoir_contribution),
									  m_i, data.jacobian);
	if (accepted) {
		curr_reservoir_contribution = data.reservoir_contribution;
		result = true;
	}
	return result;
}
