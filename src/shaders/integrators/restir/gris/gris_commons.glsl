#include "gris_commons.h"
#include "../../../commons.glsl"
layout(location = 0) rayPayloadEXT GrisHitPayload payload;
layout(location = 1) rayPayloadEXT AnyHitPayload any_hit_payload;
layout(push_constant) uniform _PushConstantRay { PCReSTIRPT pc_ray; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GrisReservoir { Reservoir d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GrisDirectLighting { vec3 d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer PrefixContributions { vec3 d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Transformation { mat4 m[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer CompactVertices { Vertex d[]; };

Transformation transforms = Transformation(scene_desc.transformations_addr);
CompactVertices compact_vertices = CompactVertices(scene_desc.compact_vertices_addr);
const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;
#define RR_MIN_DEPTH 3
uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);
uvec4 seed = init_rng(gl_LaunchIDEXT.xy, gl_LaunchSizeEXT.xy, pc_ray.total_frame_num ^ pc_ray.random_num);

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

HitData get_hitdata(vec2 attribs, uint instance_idx, uint triangle_idx) {
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
	gbuffer.n_g = (tsp_inv_to_world * vec4(cross(e0, e1), 0)).xyz;
	gbuffer.uv = vtx[0].uv0 * bary.x + vtx[1].uv0 * bary.y + vtx[2].uv0 * bary.z;
	gbuffer.material_idx = pinfo.material_index;
	return gbuffer;
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

void init_gbuffer(out GBuffer gbuffer) { gbuffer.primitive_instance_id = uvec2(-1); }

void init_reservoir(out Reservoir r) {
	r.M = 0;
	r.W = 0.0;
	r.w_sum = 0.0;
}
void init_data(out GrisData data) {
	data.rc_postfix_L = vec3(0);
	data.rc_g = 0;
	data.reservoir_contribution = vec3(0);
	data.rc_primitive_instance_id = uvec2(-1);
}

bool reservoir_data_valid(in GrisData data) { return data.rc_primitive_instance_id.y != -1; }

bool gbuffer_data_valid(in GBuffer gbuffer) { return gbuffer.primitive_instance_id.y != -1; }

vec3 uniform_sample_light(inout uvec4 seed, const Material mat, vec3 pos, const bool side, const vec3 n_s,
						  const vec3 wo, vec3 wi, float wi_len, float pdf_light_w, float pdf_light_a,
						  in LightRecord record, float cos_from_light, vec3 Le, vec3 p, bool visible) {
	vec3 res = vec3(0);

	float bsdf_pdf;
	float cos_x = dot(n_s, wi);
	vec3 f = eval_bsdf(n_s, wo, mat, 1, side, wi, bsdf_pdf, cos_x);

	if (visible && pdf_light_w > 0) {
		float mis_weight;
#if !defined(MIS) && defined(RECONNECTION)
		mis_weight = 1;
#else
		mis_weight = is_light_delta(record.flags) ? 1 : 1 / (1 + bsdf_pdf / pdf_light_w);
#endif
		res += mis_weight * f * abs(cos_x) * Le / pdf_light_w;
	}

#if defined(MIS) || !defined(RECONNECTION)
	if (get_light_type(record.flags) == LIGHT_AREA) {
		// Sample BSDF
		f = sample_bsdf(n_s, wo, mat, 1, side, wi, bsdf_pdf, cos_x, seed);
		if (bsdf_pdf != 0) {
			traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, p, tmin, wi, tmax, 0);
			if (payload.instance_idx == -1) {
				return res;
			}
			HitDataWithoutUVAndGeometryNormals gbuffer =
				get_hitdata_no_ng_uv(payload.attribs, payload.instance_idx, payload.triangle_idx);
			if (gbuffer.material_idx == record.material_idx && payload.triangle_idx == record.triangle_idx) {
				const float wi_len = length(gbuffer.pos - pos);
				const float g = abs(dot(gbuffer.n_s, -wi)) / (wi_len * wi_len);
				const float mis_weight = 1. / (1 + pdf_light_a / (g * bsdf_pdf));
				res += f * mis_weight * abs(cos_x) * Le / bsdf_pdf;
			}
		}
	}
#endif
	return res;
}

vec3 uniform_sample_light(inout uvec4 seed, const Material mat, vec3 pos, const bool side, const vec3 n_s,
						  const vec3 wo, out bool visible) {
	// Sample light
	vec3 wi;
	float wi_len;
	float pdf_light_w;
	float pdf_light_a;
	LightRecord record;
	float cos_from_light;
	const vec3 Le =
		sample_light_Li(seed, pos, pc_ray.num_lights, pdf_light_w, wi, wi_len, pdf_light_a, cos_from_light, record);
	const vec3 p = offset_ray2(pos, n_s);
	any_hit_payload.hit = 1;
	traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 1, 0, 1, p, 0, wi,
				wi_len - EPS, 1);
	visible = any_hit_payload.hit == 0;
	return uniform_sample_light(seed, mat, pos, side, n_s, wo, wi, wi_len, pdf_light_w, pdf_light_a, record,
								cos_from_light, Le, p, visible);
}

vec3 uniform_sample_light_with_visibility_override(inout uvec4 seed, const Material mat, vec3 pos, const bool side,
												   const vec3 n_s, const vec3 wo, bool visible) {
	// Sample light
	vec3 wi;
	float wi_len;
	float pdf_light_w;
	float pdf_light_a;
	LightRecord record;
	float cos_from_light;
	const vec3 Le =
		sample_light_Li(seed, pos, pc_ray.num_lights, pdf_light_w, wi, wi_len, pdf_light_a, cos_from_light, record);
	vec3 p;
#ifdef MIS
	p = offset_ray2(pos, n_s);
#endif
	return uniform_sample_light(seed, mat, pos, side, n_s, wo, wi, wi_len, pdf_light_w, pdf_light_a, record,
								cos_from_light, Le, p, visible);
}

vec3 uniform_sample_light(inout uvec4 seed, const Material mat, vec3 pos, const bool side, const vec3 n_s,
						  const vec3 wo) {
	bool unused;
	return uniform_sample_light(seed, mat, pos, side, n_s, wo, unused);
}

bool update_reservoir(inout uvec4 seed, inout Reservoir r_new, const GrisData data, float w_i) {
	r_new.w_sum += w_i;
	if (rand(seed) * r_new.w_sum < w_i) {
		r_new.data = data;
		return true;
	}
	return false;
}

bool stream_reservoir(inout uvec4 seed, inout Reservoir r_new, const GrisData data, float w_i) {
	r_new.M++;
	return update_reservoir(seed, r_new, data, w_i);
}

bool combine_reservoir(inout uvec4 seed, inout Reservoir target_reservoir, const Reservoir input_reservoir, float w) {
	float weight = w * input_reservoir.W;
	target_reservoir.M += input_reservoir.M;
	return update_reservoir(seed, target_reservoir, input_reservoir.data, weight);
}

void calc_reservoir_W(inout Reservoir r, float target_pdf) { r.W = target_pdf == 0.0 ? 0.0 : r.w_sum / target_pdf; }

float calc_target_pdf(vec3 f) { return luminance(f); }

vec3 calc_reservoir_integrand(in Reservoir r) {
	if (r.W > 0) {
		return r.data.reservoir_contribution * r.W;
	}
	return vec3(0);
}

bool is_rough(in Material mat) {
	// Only check if it's diffuse for now
	return (mat.bsdf_type & BSDF_DIFFUSE) != 0;
}

bool is_diffuse(in Material mat) { return (mat.bsdf_type & BSDF_DIFFUSE) != 0; }

uint offset(const uint pingpong) { return pingpong * pc_ray.size_x * pc_ray.size_y; }

uint set_path_flags(bool side, bool nee_visible, uint prefix_length) {
	return (prefix_length & 0x1F) << 2 | uint(nee_visible) << 1 | uint(side);
}

void unpack_path_flags(uint packed_data, out bool side, out bool nee_visible, out uint prefix_length,
					   out uint postfix_length) {
	side = (packed_data & 0x1) == 1;
	nee_visible = ((packed_data >> 1) & 0x1) == 1;
	prefix_length = (packed_data >> 2) & 0x1F;
	postfix_length = (packed_data >> 7) & 0x1F;
}

bool retrace_paths(in HitData gbuffer, in GrisData data, uvec2 source_coords, uvec2 target_coords, uint seed_helper,
				   out float jacobian, out vec3 reservoir_contribution) {
	if (!reservoir_data_valid(data)) {
		return false;
	}

	const vec2 uv = vec2(source_coords + vec2(0.5)) / vec2(gl_LaunchSizeEXT.xy);
	const vec2 d = uv * 2.0 - 1.0;
	vec3 direction = vec3(sample_camera(d));

	bool rc_side;
	bool rc_nee_visible;
	uint reservoir_prefix_length;
	uint reservoir_postfix_length;
	unpack_path_flags(data.path_flags, rc_side, rc_nee_visible, reservoir_prefix_length, reservoir_postfix_length);

	Material hit_mat = load_material(gbuffer.material_idx, gbuffer.uv);
	vec3 prefix_throughput = vec3(1);

	uint prefix_depth = 0;

	vec3 n_s = gbuffer.n_s;
	vec3 n_g = gbuffer.n_g;
	vec3 pos = gbuffer.pos;

	uvec4 reservoir_seed =
		init_rng(target_coords, gl_LaunchSizeEXT.xy, seed_helper, data.init_seed);

	HitDataWithoutGeometryNormals rc_gbuffer =
		get_hitdata_no_ng(data.rc_barycentrics, data.rc_primitive_instance_id.y, data.rc_primitive_instance_id.x);
	if (!rc_side) {
		rc_gbuffer.n_s *= -1;
	}
	while (true) {
		if ((prefix_depth + reservoir_postfix_length) >= pc_ray.max_depth - 1) {
			return false;
		}

		vec3 wo = -direction;
		bool side = true;
		if (dot(n_g, wo) < 0.) n_g = -n_g;
		if (dot(n_g, n_s) < 0) {
			n_s = -n_s;
			side = false;
		}
		vec3 origin = offset_ray(pos, n_g);

		vec3 wi = rc_gbuffer.pos - pos;
		float wi_len = length(wi);
		wi /= wi_len;

		bool connectable = is_rough(hit_mat) && wi_len > pc_ray.min_vertex_distance_ratio * pc_ray.scene_extent;
		connectable = connectable && prefix_depth >= (reservoir_prefix_length - 1);
		bool connected = false;
		if (connectable) {
			vec3 p = offset_ray2(pos, n_s);
			any_hit_payload.hit = 1;
			traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 1, 0, 1, p,
						0, wi, wi_len - EPS, 1);
			connected = any_hit_payload.hit == 0;
		}

		if (connected) {
			float rc_pdf;
			float cos_x = dot(n_s, wi);
			vec3 rc_f = eval_bsdf(n_s, wo, hit_mat, 1, side, wi, rc_pdf, cos_x);
			if (rc_pdf == 0) {
				return false;
			}
			// Compute the partial F
			const vec3 partial_throughput = prefix_throughput * rc_f * abs(cos_x);
			// Compute the Jacobian
			const float g_curr = abs(dot(rc_gbuffer.n_s, wi)) / (wi_len * wi_len);
			jacobian = data.rc_g == 0.0 ? 0.0 : g_curr / data.rc_g;
			// Compute the direct lighting on the reconnection vertex
			const Material rc_mat = load_material(rc_gbuffer.material_idx, rc_gbuffer.uv);

			if (/*is_diffuse(rc_mat)*/false) { // TODO: This causes some bias for some reason.
				reservoir_contribution = data.rc_Li;
			} else {
				const float light_pick_pdf = 1. / pc_ray.light_triangle_count;
				uvec4 reconnection_seed = init_rng(target_coords, gl_LaunchSizeEXT.xy,
												   seed_helper, data.rc_seed);
				reservoir_contribution =
					uniform_sample_light_with_visibility_override(reconnection_seed, rc_mat, rc_gbuffer.pos, rc_side,
																  rc_gbuffer.n_s, -wi, rc_nee_visible) /
					light_pick_pdf;
			}

			float rc_cos_x = dot(rc_gbuffer.n_s, data.rc_wi);
			float bsdf_pdf;
			const vec3 f = eval_bsdf(rc_gbuffer.n_s, wo, rc_mat, 1, rc_side, data.rc_wi, bsdf_pdf, rc_cos_x);
			if (bsdf_pdf > 0) {
				reservoir_contribution += f * abs(rc_cos_x) * data.rc_postfix_L / bsdf_pdf;
			}
			reservoir_contribution = partial_throughput * reservoir_contribution;
			// reservoir_contribution = partial_throughput * data.rc_postfix_L;
			return true;
		}

		float pdf, cos_theta;
		const vec3 f =
			sample_bsdf(n_s, wo, hit_mat, 1 /*radiance=cam*/, side, direction, pdf, cos_theta, reservoir_seed);
		if (pdf == 0) {
			return false;
		}

		prefix_throughput *= f * abs(cos_theta) / pdf;

		traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, origin, tmin, direction, tmax, 0);
		const bool found_isect = payload.instance_idx != -1;
		if (!found_isect) {
			return false;
		}
		HitData gbuffer = get_hitdata(payload.attribs, payload.instance_idx, payload.triangle_idx);
		hit_mat = load_material(gbuffer.material_idx, gbuffer.uv);

		n_s = gbuffer.n_s;
		n_g = gbuffer.n_g;
		pos = gbuffer.pos;

		prefix_depth++;
	}

	return false;
}

bool retrace_paths_and_evaluate(in HitData gbuffer, in GrisData data, uvec2 source_coords, uvec2 target_coords,
								uint seed_helper, out float target_pdf) {
	vec3 reservoir_contribution;
	float jacobian;
	bool result = retrace_paths(gbuffer, data, source_coords, target_coords, seed_helper, jacobian, reservoir_contribution);
	target_pdf = result ? calc_target_pdf(reservoir_contribution) * jacobian : 0.0;
	return result;
}