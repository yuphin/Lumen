#include "gris_commons.h"
#include "../../../commons.glsl"
layout(location = 0) rayPayloadEXT GrisHitPayload payload;
layout(location = 1) rayPayloadEXT AnyHitPayload any_hit_payload;
layout(push_constant) uniform _PushConstantRay { PCReSTIRPT pc; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GrisReservoir { Reservoir d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GrisDirectLighting { vec3 d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer PrefixContributions { vec3 d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Transformation { mat4 m[]; };

Transformation transforms = Transformation(scene_desc.transformations_addr);
const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;
#define RR_MIN_DEPTH 3
uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);

#define RECONNECTION_TYPE_INVALID 0
#define RECONNECTION_TYPE_NEE 1
#define RECONNECTION_TYPE_EMISSIVE_AFTER_RC 2
#define RECONNECTION_TYPE_EMISSIVE 3
#define RECONNECTION_TYPE_DEFAULT 4

#define STREAMING_MODE_INDIVIDUAL 0
#define STREAMING_MODE_SPLIT 1

#ifndef STREAMING_MODE
#define STREAMING_MODE STREAMING_MODE_INDIVIDUAL
#endif	// STREAMING_MODE

struct HitData {
	vec3 pos;
	vec3 n_g;
	vec3 n_s;
	vec2 uv;
	uint material_idx;
};

struct HitDataWithoutUVAndGeometryNormals {
	vec3 pos;
	vec3 n_s;
	uint material_idx;
};

struct HitDataWithoutGeometryNormals {
	vec3 pos;
	vec3 n_s;
	uint material_idx;
	vec2 uv;
};

ivec2 get_neighbor_offset(inout uvec4 seed) {
	const float randa = rand(seed) * 2 * PI;
	const float randr = sqrt(rand(seed)) * pc.spatial_radius;
	return ivec2(floor(cos(randa) * randr), floor(sin(randa) * randr));
}

HitData get_hitdata(vec2 attribs, uint instance_idx, uint triangle_idx, out float area) {
	const PrimMeshInfo pinfo = prim_infos.d[instance_idx];
	const uint index_offset = pinfo.index_offset + 3 * triangle_idx;
	const ivec3 ind = ivec3(pinfo.vertex_offset) +
					  ivec3(indices.i[index_offset + 0], indices.i[index_offset + 1], indices.i[index_offset + 2]);
	const vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
	const mat4 to_world = transforms.m[instance_idx];
	const mat4 tsp_inv_to_world = transpose(inverse(to_world));

	HitData gbuffer;
	Vertex vtx[3];

	vtx[0] = compact_vertices.d[ind.x];
	vtx[1] = compact_vertices.d[ind.y];
	vtx[2] = compact_vertices.d[ind.z];

	gbuffer.pos = vec3(to_world * vec4(vtx[0].pos * bary.x + vtx[1].pos * bary.y + vtx[2].pos * bary.z, 1.0));
	gbuffer.n_s =
		vec3(tsp_inv_to_world * vec4(vtx[0].normal * bary.x + vtx[1].normal * bary.y + vtx[2].normal * bary.z, 1.0));
	const vec3 e0 = vtx[2].pos - vtx[0].pos;
	const vec3 e1 = vtx[1].pos - vtx[0].pos;

	const vec4 e0t = to_world * vec4(vtx[2].pos - vtx[0].pos, 0);
	const vec4 e1t = to_world * vec4(vtx[1].pos - vtx[0].pos, 0);
	area = 0.5 * length(cross(vec3(e0t), vec3(e1t)));
	gbuffer.n_g = (tsp_inv_to_world * vec4(cross(e0, e1), 0)).xyz;
	gbuffer.uv = vtx[0].uv0 * bary.x + vtx[1].uv0 * bary.y + vtx[2].uv0 * bary.z;
	gbuffer.material_idx = pinfo.material_index;
	return gbuffer;
}

HitData get_hitdata(vec2 attribs, uint instance_idx, uint triangle_idx) {
	float unused;
	return get_hitdata(attribs, instance_idx, triangle_idx, unused);
}

HitDataWithoutUVAndGeometryNormals get_hitdata_no_ng_uv(vec2 attribs, uint instance_idx, uint triangle_idx) {
	const PrimMeshInfo pinfo = prim_infos.d[instance_idx];
	const uint index_offset = pinfo.index_offset + 3 * triangle_idx;
	const ivec3 ind = ivec3(pinfo.vertex_offset) +
					  ivec3(indices.i[index_offset + 0], indices.i[index_offset + 1], indices.i[index_offset + 2]);
	const vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
	const mat4 to_world = transforms.m[instance_idx];
	const mat4 tsp_inv_to_world = transpose(inverse(to_world));
	HitDataWithoutUVAndGeometryNormals gbuffer;
	Vertex vtx[3];

	vtx[0] = compact_vertices.d[ind.x];
	vtx[1] = compact_vertices.d[ind.y];
	vtx[2] = compact_vertices.d[ind.z];
	gbuffer.pos = vec3(to_world * vec4(vtx[0].pos * bary.x + vtx[1].pos * bary.y + vtx[2].pos * bary.z, 1.0));
	gbuffer.n_s =
		vec3(tsp_inv_to_world * vec4(vtx[0].normal * bary.x + vtx[1].normal * bary.y + vtx[2].normal * bary.z, 1.0));
	gbuffer.material_idx = pinfo.material_index;
	return gbuffer;
}

HitDataWithoutGeometryNormals get_hitdata_no_ng(vec2 attribs, uint instance_idx, uint triangle_idx) {
	const PrimMeshInfo pinfo = prim_infos.d[instance_idx];
	const uint index_offset = pinfo.index_offset + 3 * triangle_idx;
	const ivec3 ind = ivec3(pinfo.vertex_offset) +
					  ivec3(indices.i[index_offset + 0], indices.i[index_offset + 1], indices.i[index_offset + 2]);
	const vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
	const mat4 to_world = transforms.m[instance_idx];
	const mat4 tsp_inv_to_world = transpose(inverse(to_world));
	HitDataWithoutGeometryNormals gbuffer;
	Vertex vtx[3];

	vtx[0] = compact_vertices.d[ind.x];
	vtx[1] = compact_vertices.d[ind.y];
	vtx[2] = compact_vertices.d[ind.z];
	gbuffer.pos = vec3(to_world * vec4(vtx[0].pos * bary.x + vtx[1].pos * bary.y + vtx[2].pos * bary.z, 1.0));
	gbuffer.n_s =
		vec3(tsp_inv_to_world * vec4(vtx[0].normal * bary.x + vtx[1].normal * bary.y + vtx[2].normal * bary.z, 1.0));
	gbuffer.material_idx = pinfo.material_index;
	gbuffer.uv = vtx[0].uv0 * bary.x + vtx[1].uv0 * bary.y + vtx[2].uv0 * bary.z;
	return gbuffer;
}

vec3 get_hitdata_pos_only(vec2 attribs, uint instance_idx, uint triangle_idx) {
	const PrimMeshInfo pinfo = prim_infos.d[instance_idx];
	const uint index_offset = pinfo.index_offset + 3 * triangle_idx;
	const ivec3 ind = ivec3(pinfo.vertex_offset) +
					  ivec3(indices.i[index_offset + 0], indices.i[index_offset + 1], indices.i[index_offset + 2]);
	const vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
	const mat4 to_world = transforms.m[instance_idx];
	const mat4 tsp_inv_to_world = transpose(inverse(to_world));
	HitDataWithoutGeometryNormals gbuffer;
	Vertex vtx[3];

	vtx[0] = compact_vertices.d[ind.x];
	vtx[1] = compact_vertices.d[ind.y];
	vtx[2] = compact_vertices.d[ind.z];
	return vec3(to_world * vec4(vtx[0].pos * bary.x + vtx[1].pos * bary.y + vtx[2].pos * bary.z, 1.0));
}

vec3 do_nee(inout uvec4 seed, vec3 pos, Material hit_mat, bool side, vec3 n_s, vec3 wo, bool trace, out vec3 light_pos,
			out bool is_directional_light) {
	float pdf_light_a;
	float pdf_light_w;
	vec3 wi = vec3(0);
	float wi_len = 0;
	LightRecord record;
	float cos_from_light;
	vec4 rands = vec4(rand(seed), rand(seed), rand(seed), rand(seed));
	const vec3 Le =
		sample_light_Li(rands, pos, pc.num_lights, pdf_light_w, wi, wi_len, pdf_light_a, cos_from_light, record);
	const vec3 p = offset_ray2(pos, n_s);
	float light_bsdf_pdf;
	float cos_x = dot(n_s, wi);
	vec3 f_light = eval_bsdf(n_s, wo, hit_mat, 1, side, wi, light_bsdf_pdf);
	any_hit_payload.hit = 1;
	bool visible;

	if (!trace) {
		visible = true;
	} else {
		traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 1, 0, 1, p, 0,
					wi, wi_len - EPS, 1);
		visible = any_hit_payload.hit == 0;
	}
	is_directional_light = get_light_type(record.flags) == LIGHT_DIRECTIONAL;
	if (is_directional_light) {
		light_pos = wi * wi_len;
	} else {
		light_pos = pos + wi * wi_len;
	}

	const float light_pick_pdf = 1. / pc.light_triangle_count;
	if (visible && pdf_light_w > 0) {
		const float mis_weight = is_light_delta(record.flags) ? 1 : 1 / (1 + light_bsdf_pdf / pdf_light_w);
		return mis_weight * f_light * abs(cos_x) * Le / (light_pick_pdf * pdf_light_w);
	}
	return vec3(0);
}

vec3 do_nee(inout uvec4 seed, vec3 pos, Material hit_mat, bool side, vec3 n_s, vec3 wo, bool trace) {
	vec3 unused_pos;
	bool unused_bool;
	return do_nee(seed, pos, hit_mat, side, n_s, wo, trace, unused_pos, unused_bool);
}

void init_gbuffer(out GBuffer gbuffer) {
	gbuffer.barycentrics = vec2(0);
	gbuffer.primitive_instance_id = uvec2(-1);
}

void init_data(out GrisData data) {
	data.path_flags = 0;
	data.rc_primitive_instance_id = uvec2(-1);
}

void init_reservoir(out Reservoir r) {
	init_data(r.data);
	r.M = 0;
	r.W = 0.0;
	r.w_sum = 0.0;
	r.target_pdf = 0.0;
}

bool reservoir_data_valid(in GrisData data) { return data.path_flags != 0; }

bool gbuffer_data_valid(in GBuffer gbuffer) { return gbuffer.primitive_instance_id.y != -1; }

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
	return (mat.bsdf_type & BSDF_TYPE_DIFFUSE) != 0;
}

uint offset(const uint pingpong) { return pingpong * pc.size_x * pc.size_y; }

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

bool retrace_paths(in HitData dst_gbuffer, in GrisData data, vec3 dst_wi, float src_jacobian, out float jacobian_out,
				   out vec3 reservoir_contribution, out float jacobian_num) {
	// Note: The source reservoir always corresponds to the canonical reservoir because retracing only happens on
	// pairwise mode
	reservoir_contribution = vec3(0);
	jacobian_out = 0;
	jacobian_num = 0;
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

	HitData rc_gbuffer;
	Material rc_hit_mat;

	// Unconditionally initializing here causes corruption when rc_type is NEE
	if (rc_type != RECONNECTION_TYPE_NEE) {
		rc_gbuffer =
			get_hitdata(data.rc_barycentrics, data.rc_primitive_instance_id.y, data.rc_primitive_instance_id.x);
		rc_hit_mat = load_material(rc_gbuffer.material_idx, rc_gbuffer.uv);
	}

	uint prefix_depth = 0;
	vec3 prefix_throughput = vec3(1);

	float prefix_jacobian = 1.0;

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
		dst_gbuffer.pos = offset_ray(dst_gbuffer.pos, dst_gbuffer.n_g);

		Material dst_hit_mat = load_material(dst_gbuffer.material_idx, dst_gbuffer.uv);

		if (rc_type != RECONNECTION_TYPE_NEE) {
			float rc_wi_len = length(rc_gbuffer.pos - dst_gbuffer.pos);
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

			vec3 dst_postfix_wi;
			if (rc_type == RECONNECTION_TYPE_NEE) {
				dst_postfix_wi = is_directional_light ? data.rc_Li : data.rc_Li - dst_gbuffer.pos;
			} else {
				dst_postfix_wi = rc_gbuffer.pos - dst_gbuffer.pos;
			}

			float wi_len_sqr = dot(dst_postfix_wi, dst_postfix_wi);
			float wi_len = sqrt(wi_len_sqr);
			dst_postfix_wi /= wi_len;
			vec3 p = offset_ray2(is_directional_light ? org_pos : dst_gbuffer.pos, dst_gbuffer.n_s);
			any_hit_payload.hit = 1;
			traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 1, 0, 1, p,
						0, dst_postfix_wi, wi_len - EPS, 1);
			if (any_hit_payload.hit == 1) {
				return false;
			}

			set_bounce_flag(bounce_flags, prefix_depth + 1, true);
			if (bounce_flags != (data.path_flags & 0xFFFF0000)) {
				return false;
			}

			if (rc_type == RECONNECTION_TYPE_NEE) {
				// In this case directly re-use the NEE result
				ASSERT(prefix_depth != 0);	// Can't process direct lighting
				uvec2 rc_coords = uvec2(data.rc_coords / gl_LaunchSizeEXT.y, data.rc_coords % gl_LaunchSizeEXT.y);
				uvec4 reconnection_seed = init_rng(rc_coords, gl_LaunchSizeEXT.xy, data.seed_helpers.x, data.rc_seed);
				ASSERT(reconnection_seed == data.debug_seed);

				vec3 Li =
					do_nee(reconnection_seed, dst_gbuffer.pos, dst_hit_mat, dst_side, dst_gbuffer.n_s, dst_wo, false);
				jacobian_num = prefix_jacobian;
				jacobian = jacobian_num / src_jacobian;
				reservoir_contribution = prefix_throughput * Li;
				LOG_CLICKED2("NEE: %d - %d\n", prefix_depth, (data.path_flags) >> 16);
			} else {
				ASSERT(rc_type != RECONNECTION_TYPE_NEE);

				float g = abs(dot(dst_postfix_wi, rc_gbuffer.n_s)) / wi_len_sqr;

				float dst_postfix_pdf;
				float rc_cos_x = dot(dst_gbuffer.n_s, dst_postfix_wi);
				vec3 dst_postfix_f = eval_bsdf(dst_gbuffer.n_s, dst_wo, dst_hit_mat, 1, dst_side, dst_postfix_wi,
											   dst_postfix_pdf);

				ASSERT(rc_type == RECONNECTION_TYPE_DEFAULT || rc_type == RECONNECTION_TYPE_EMISSIVE_AFTER_RC);
				jacobian_num = dst_postfix_pdf * prefix_jacobian * g;
				jacobian = jacobian_num / src_jacobian;

				float mis_weight = 1.0;
				if (rc_type == RECONNECTION_TYPE_EMISSIVE_AFTER_RC) {
					mis_weight = 1.0 / (1 + uintBitsToFloat(data.rc_seed) / dst_postfix_pdf);
				}
				reservoir_contribution =
					prefix_throughput * abs(rc_cos_x) * dst_postfix_f * data.rc_Li * mis_weight / dst_postfix_pdf;
				LOG_CLICKED3("Default: %d - %d - %d\n", prefix_depth, rc_postfix_length, rc_type);
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
		const vec3 f = sample_bsdf(dst_gbuffer.n_s, dst_wo, dst_hit_mat, 1 /*radiance=cam*/, dst_side, dst_wi, pdf,
								   cos_theta, reservoir_seed);

		if (pdf == 0) {
			return false;
		}

		prefix_jacobian *= pdf;
		prefix_throughput *= f * abs(cos_theta) / pdf;

		traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, dst_gbuffer.pos, tmin, dst_wi, tmax, 0);
		const bool found_isect = payload.instance_idx != -1;
		if (!found_isect) {
			return false;
		}

		vec3 prev_pos = dst_gbuffer.pos;
		dst_gbuffer = get_hitdata(payload.attribs, payload.instance_idx, payload.triangle_idx);
		prev_far = length(dst_gbuffer.pos - prev_pos) > pc.min_vertex_distance_ratio * pc.scene_extent;
		prefix_depth++;
	}
}

bool retrace_paths(in HitData dst_gbuffer, in GrisData data, vec3 dst_wi, out float jacobian_out,
				   out vec3 reservoir_contribution) {
	float unused_jacobian;
	return retrace_paths(dst_gbuffer, data, dst_wi, data.rc_partial_jacobian, jacobian_out, reservoir_contribution,
						 unused_jacobian);
}

bool retrace_paths_and_evaluate(in HitData dst_gbuffer, in GrisData data, vec3 dst_wi, float src_jacobian,
								out float target_pdf) {
	vec3 reservoir_contribution;
	float jacobian = 0;
	float unused_jacobian_num;
	bool result =
		retrace_paths(dst_gbuffer, data, dst_wi, src_jacobian, jacobian, reservoir_contribution, unused_jacobian_num);
	target_pdf = calc_target_pdf(reservoir_contribution) * jacobian;
	return result;
}

bool retrace_paths_and_evaluate(in HitData dst_gbuffer, in GrisData data, vec3 dst_wi, out float target_pdf) {
	return retrace_paths_and_evaluate(dst_gbuffer, data, dst_wi, data.rc_partial_jacobian, target_pdf);
}

bool process_reservoir(inout uvec4 seed, inout Reservoir reservoir, inout float m_c, in Reservoir canonical_reservoir,
					   in Reservoir source_reservoir, in ReconnectionData data, ivec2 neighbor_coords,
					   float canonical_in_canonical_pdf, uint num_spatial_samples,
					   inout vec3 curr_reservoir_contribution) {
	source_reservoir.M = min(source_reservoir.M, 20);

	bool result = false;

	float neighbor_in_neighbor_pdf = calc_target_pdf(source_reservoir.data.reservoir_contribution);
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

	ASSERT1(m_c_val > -1e-3 && m_c_val <= 1.001, "m_c_val <= 1.0 : %f\n", m_c_val);
	ASSERT1(m_i > -1e-3 && m_i <= 1.001, "m_i <= 1.0 : %f\n", m_i);

	bool accepted = combine_reservoir(seed, reservoir, source_reservoir, calc_target_pdf(data.reservoir_contribution),
									  m_i, data.jacobian);
	if (accepted) {
		curr_reservoir_contribution = data.reservoir_contribution;
		result = true;
	}
	return result;
}