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

vec3 do_nee(inout uvec4 seed, HitData gbuffer, Material hit_mat, bool side, vec3 n_s, vec3 wo, out vec3 wi,
			bool trace_ray) {
	float wi_len;
	float pdf_light_a;
	float pdf_light_w;
	LightRecord record;
	float cos_from_light;
	const vec3 Le = sample_light_Li(seed, gbuffer.pos, pc_ray.num_lights, pdf_light_w, wi, wi_len, pdf_light_a,
									cos_from_light, record);
	const vec3 p = offset_ray2(gbuffer.pos, n_s);
	float light_bsdf_pdf;
	float cos_x = dot(n_s, wi);
	vec3 f_light = eval_bsdf(n_s, wo, hit_mat, 1, side, wi, light_bsdf_pdf, cos_x);
	any_hit_payload.hit = 1;
	bool visible;
	if (!trace_ray) {
		visible = true;
	} else {
		traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 1, 0, 1, p, 0,
					wi, wi_len - EPS, 1);
		visible = any_hit_payload.hit == 0;
	}
	const float light_pick_pdf = 1. / pc_ray.light_triangle_count;
	if (visible && pdf_light_w > 0) {
		const float mis_weight = is_light_delta(record.flags) ? 1 : 1 / (1 + light_bsdf_pdf / pdf_light_w);
		return mis_weight * f_light * abs(cos_x) * Le / (pdf_light_w * light_pick_pdf);
	}
	return vec3(0);
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

void init_gbuffer(out GBuffer gbuffer) { gbuffer.primitive_instance_id = uvec2(-1); }

void init_reservoir(out Reservoir r) {
	r.M = 0;
	r.W = 0.0;
	r.w_sum = 0.0;
}
void init_data(out GrisData data) { data.rc_primitive_instance_id = uvec2(-1); }

bool reservoir_data_valid(in GrisData data) { return data.rc_primitive_instance_id.y != -1; }

bool gbuffer_data_valid(in GBuffer gbuffer) { return gbuffer.primitive_instance_id.y != -1; }

bool update_reservoir(inout uvec4 seed, inout Reservoir r_new, const GrisData data, float target_pdf,
					  float source_pdf) {
	float w_i = target_pdf * source_pdf;
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
	float inv_source_pdf = mis_weight * jacobian * input_reservoir.W;
	if (isnan(inv_source_pdf) || inv_source_pdf == 0.0) {
		return false;
	}
	target_reservoir.M += input_reservoir.M;
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
	return (mat.bsdf_type & BSDF_DIFFUSE) != 0;
}

bool is_diffuse(in Material mat) { return (mat.bsdf_type & BSDF_DIFFUSE) != 0; }

uint offset(const uint pingpong) { return pingpong * pc_ray.size_x * pc_ray.size_y; }

uint set_path_flags(uint prefix_length, uint postfix_length, bool is_nee) {
	return (postfix_length & 0x1F) << 6 | (prefix_length & 0x1F) << 1 | uint(is_nee);
}

void unpack_path_flags(uint packed_data, out bool is_nee, out uint prefix_length, out uint postfix_length) {
	is_nee = (packed_data & 0x1) == 1;
	prefix_length = (packed_data >> 1) & 0x1F;
	postfix_length = (packed_data >> 6) & 0x1F;
}

vec3 get_primary_direction(uvec2 coords) {
	const vec2 uv = vec2(coords + vec2(0.5)) / vec2(gl_LaunchSizeEXT.xy);
	const vec2 d = uv * 2.0 - 1;
	return normalize(sample_camera(d).xyz);
}

bool reconnect_paths(in HitData dst_gbuffer, in HitData src_gbuffer, in GrisData data, uvec2 src_coords,
					 uvec2 dst_coords, uint seed_helper, out float jacobian_out, out vec3 reservoir_contribution) {
	reservoir_contribution = vec3(0);
	jacobian_out = 0;
	float jacobian = 0;
	if (!reservoir_data_valid(data)) {
		return false;
	}
	vec3 dst_wo = -get_primary_direction(dst_coords);
	vec3 src_wo = -get_primary_direction(src_coords);

	bool is_nee;
	uint rc_prefix_length;
	uint rc_postfix_length;
	unpack_path_flags(data.path_flags, is_nee, rc_prefix_length, rc_postfix_length);

	Material dst_hit_mat = load_material(dst_gbuffer.material_idx, dst_gbuffer.uv);

	HitData rc_gbuffer =
		get_hitdata(data.rc_barycentrics, data.rc_primitive_instance_id.y, data.rc_primitive_instance_id.x);

	bool dst_side = face_forward(dst_gbuffer.n_s, dst_gbuffer.n_g, dst_wo);
	bool src_side = face_forward(src_gbuffer.n_s, src_gbuffer.n_g, src_wo);

	vec3 dst_wi = rc_gbuffer.pos - dst_gbuffer.pos;
	float wi_len = length(dst_wi);
	dst_wi /= wi_len;

	if (is_rough(dst_hit_mat)) {
		vec3 p = offset_ray2(dst_gbuffer.pos, dst_gbuffer.n_s);
		any_hit_payload.hit = 1;
		traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 1, 0, 1, p, 0,
					dst_wi, wi_len - EPS, 1);
		bool connected = any_hit_payload.hit == 0;
		if (!connected) {
			return false;
		}
	} else {
		return false;
	}

	// Adjust normals from dst to reconnection
	vec3 rc_n_s = rc_gbuffer.n_s;
	vec3 rc_n_g = rc_gbuffer.n_g;
	bool rc_side = face_forward(rc_n_s, rc_n_g, -dst_wi);
	float g_dst = abs(dot(rc_n_s, -dst_wi)) / (wi_len * wi_len);
	// The "source" location does not necessarily mean the canonical location
	vec3 rc_n_s_src = rc_gbuffer.n_s;
	vec3 rc_n_g_src = rc_gbuffer.n_g;
	vec3 rc_wo_src = -(rc_gbuffer.pos - src_gbuffer.pos);
	float rc_wo_src_len = length(rc_wo_src);
	rc_wo_src /= rc_wo_src_len;
	bool rc_side_src = face_forward(rc_n_s_src, rc_n_g_src, rc_wo_src);
	float g_src = abs(dot(rc_n_s_src, rc_wo_src)) / (rc_wo_src_len * rc_wo_src_len);
	jacobian = g_dst / g_src;
	// Correction for solid angle Jacobians
	float dst_pdf;
	float cos_x = dot(dst_gbuffer.n_s, dst_wi);
	vec3 dst_f = eval_bsdf(dst_gbuffer.n_s, dst_wo, dst_hit_mat, 1, dst_side, dst_wi, dst_pdf, cos_x);
	if (dst_pdf == 0.0) {
		return false;
	}
	Material src_hit_mat = load_material(src_gbuffer.material_idx, src_gbuffer.uv);
	float src_pdf = bsdf_pdf(src_hit_mat, src_gbuffer.n_s, src_wo, -rc_wo_src);

	jacobian *= dst_pdf / src_pdf;
	if (is_nee) {
		// The NEE PDF does not depend on the surface, only MIS weight does
		// Also we do not need to trace visibility ray since we know this was accepted into the reservoir
		uvec2 rc_coords = uvec2(data.rc_coords / gl_LaunchSizeEXT.y, data.rc_coords % gl_LaunchSizeEXT.y);
		uvec4 reconnection_seed = init_rng(rc_coords, gl_LaunchSizeEXT.xy, seed_helper, data.rc_seed);
		vec3 dst_L_wi;
		uvec4 debug_seed = reconnection_seed;
		vec3 Li =
			do_nee(reconnection_seed, dst_gbuffer, dst_hit_mat, dst_side, dst_gbuffer.n_s, dst_wo, dst_L_wi, false);
		ASSERT(debug_seed == data.debug_seed);
		reservoir_contribution = dst_f * abs(cos_x) * Li;
	} else {
		ASSERT(data.rc_seed == -1);
		Material rc_hit_mat = load_material(rc_gbuffer.material_idx, rc_gbuffer.uv);
		float dst_postfix_pdf;
		float rc_cos_x = dot(rc_n_s, data.rc_wi);
		vec3 dst_postfix_f = eval_bsdf(rc_n_s, -dst_wi, rc_hit_mat, 1, rc_side, data.rc_wi, dst_postfix_pdf, rc_cos_x);

		float src_postfix_pdf = bsdf_pdf(rc_hit_mat, rc_n_s_src, rc_wo_src, data.rc_wi);
		jacobian *= dst_postfix_pdf / src_postfix_pdf;

		reservoir_contribution = dst_f * abs(cos_x) * dst_postfix_f * abs(rc_cos_x) * data.rc_Li;
	}

	if (isnan(jacobian) || isinf(jacobian) || jacobian == 0) {
		reservoir_contribution = vec3(0);
		return false;
	}
	jacobian_out = jacobian;

	return true;
}

bool reconnect_paths_and_evaluate(in HitData dst_gbuffer, in HitData src_gbuffer, in GrisData data, uvec2 src_coords,
								  uvec2 dst_coords, uint seed_helper, out float target_pdf) {
	vec3 reservoir_contribution;
	float jacobian;
	bool result = reconnect_paths(dst_gbuffer, src_gbuffer, data, src_coords, dst_coords, seed_helper, jacobian,
								  reservoir_contribution);
	target_pdf = (result && jacobian > 0) ? calc_target_pdf(reservoir_contribution) * jacobian : 0.0;
	return result;
}