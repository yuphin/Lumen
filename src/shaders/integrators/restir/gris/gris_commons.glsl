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

	const vec4 e0t = to_world * vec4(vtx[2].pos - vtx[0].pos,0);
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
void init_data(out GrisData data) {
	data.rc_primitive_instance_id = uvec2(-1);
}

bool reservoir_data_valid(in GrisData data) { return data.rc_primitive_instance_id.y != -1; }

bool gbuffer_data_valid(in GBuffer gbuffer) { return gbuffer.primitive_instance_id.y != -1; }


bool update_reservoir(inout uvec4 seed, inout Reservoir r_new, const GrisData data, float target_pdf, float source_pdf) {
	float w_i = target_pdf * source_pdf;
	r_new.w_sum += w_i;
	if (rand(seed) * r_new.w_sum < w_i) {
		r_new.data = data;
		r_new.target_pdf = target_pdf;
		return true;
	}
	return false;
}


bool stream_reservoir(inout uvec4 seed, inout Reservoir r_new, const GrisData data, float target_pdf, float inv_source_pdf) {
	r_new.M++;
	if(isnan(inv_source_pdf) || inv_source_pdf == 0.0) {
		return false;
	}
	return update_reservoir(seed, r_new, data, target_pdf, inv_source_pdf);
}

bool combine_reservoir(inout uvec4 seed, inout Reservoir target_reservoir, const Reservoir input_reservoir, float target_pdf, float mis_weight, float jacobian) {
	float inv_source_pdf = mis_weight * jacobian * input_reservoir.W;
	if(isnan(inv_source_pdf) || inv_source_pdf == 0.0) {
		return false;
	}
	target_reservoir.M += input_reservoir.M;
	return update_reservoir(seed, target_reservoir, input_reservoir.data, target_pdf, inv_source_pdf);
}

void calc_reservoir_W(inout Reservoir r) { 
	float denom = r.target_pdf * r.M;
	r.W = denom == 0.0 ? 0.0 : r.w_sum / denom;
}

void calc_reservoir_W_with_mis(inout Reservoir r) { 
	r.W = r.target_pdf == 0.0 ? 0.0 : r.w_sum / r.target_pdf; 
}

float calc_target_pdf(vec3 f) { return luminance(f); }

bool is_rough(in Material mat) {
	// Only check if it's diffuse for now
	return (mat.bsdf_type & BSDF_DIFFUSE) != 0;
}

bool is_diffuse(in Material mat) { return (mat.bsdf_type & BSDF_DIFFUSE) != 0; }

uint offset(const uint pingpong) { return pingpong * pc_ray.size_x * pc_ray.size_y; }


uint set_path_flags(uint prefix_length, uint postfix_length, bool is_nee) {
	return (postfix_length & 0x1F) << 6 |  (prefix_length & 0x1F) << 1 | uint(is_nee);
}

void unpack_path_flags(uint packed_data, out bool side, out bool nee_visible, out uint prefix_length,
					   out uint postfix_length) {
	side = (packed_data & 0x1) == 1;
	nee_visible = ((packed_data >> 1) & 0x1) == 1;
	prefix_length = (packed_data >> 2) & 0x1F;
	postfix_length = (packed_data >> 7) & 0x1F;
}


bool reconnect_paths(in HitData dst_gbuffer, in HitData src_gbuffer, in GrisData data, uvec2 src_coords,
					uvec2 dst_coords, uint seed_helper, out float jacobian_out, out vec3 reservoir_contribution) {
#if 0
	reservoir_contribution = vec3(0);
	jacobian_out = 0;
	float jacobian = 0;
	if (!reservoir_data_valid(data)) {
		return false;
	}
	const vec2 dst_uv = vec2(dst_coords + vec2(0.5)) / vec2(gl_LaunchSizeEXT.xy);
	const vec2 d = dst_uv * 2.0 - 1.0;
	vec3 dst_wo = -normalize(vec3(sample_camera(d)));

	const vec2 src_uv = vec2(src_coords + vec2(0.5)) / vec2(gl_LaunchSizeEXT.xy);
	const vec2 src_d = src_uv * 2.0 - 1.0;
	vec3 src_wo = -normalize(vec3(sample_camera(src_d)));

	bool rc_side;
	bool rc_nee_visible;
	uint reservoir_prefix_length;
	uint reservoir_postfix_length;
	unpack_path_flags(data.path_flags, rc_side, rc_nee_visible, reservoir_prefix_length, reservoir_postfix_length);

	Material hit_mat = load_material(dst_gbuffer.material_idx, dst_gbuffer.uv);
	uint prefix_depth = 0;

	uvec4 reservoir_seed = init_rng(src_coords, gl_LaunchSizeEXT.xy, seed_helper, data.init_seed);

	HitData rc_src_gbuffer =
		get_hitdata(data.rc_barycentrics, data.rc_primitive_instance_id.y, data.rc_primitive_instance_id.x);
	
	bool side = face_forward(dst_gbuffer.n_s, dst_gbuffer.n_g, dst_wo);

	vec3 dst_wi = rc_src_gbuffer.pos - dst_gbuffer.pos;
	float wi_len = length(dst_wi);
	dst_wi /= wi_len;

	rc_side = face_forward(rc_src_gbuffer.n_s, rc_src_gbuffer.n_g, -dst_wi);

	bool connectable =  is_rough(hit_mat);
	bool connected = false;
	if (connectable) {
		vec3 p = offset_ray2(dst_gbuffer.pos, dst_gbuffer.n_s);
		any_hit_payload.hit = 1;
		traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xFF, 1, 0, 1, p, 0,
					dst_wi, wi_len - EPS, 1);
		connected = any_hit_payload.hit == 0;
	}

	if (connected) {
		float rc_pdf;
		float cos_x = dot(dst_gbuffer.n_s, dst_wi);
		vec3 rc_f = eval_bsdf(dst_gbuffer.n_s, dst_wo, hit_mat, 1, side, dst_wi, rc_pdf, cos_x);
		if (rc_pdf == 0.0) {
			return false;
		}
		// Compute the partial F
		const vec3 partial_throughput = rc_f * abs(cos_x);
		// Compute the Jacobian
		const float g_curr = abs(dot(rc_src_gbuffer.n_s, -dst_wi)) / (wi_len * wi_len);

		jacobian = g_curr;
		jacobian *= rc_pdf;

		vec3 src_wi = rc_src_gbuffer.pos - src_gbuffer.pos;
		float len_src_wi_sqr = dot(src_wi, src_wi);
		src_wi /= sqrt(len_src_wi_sqr);
		float g_source = abs(dot(rc_src_gbuffer.n_s, -src_wi)) / len_src_wi_sqr;
		bool src_side = face_forward(src_gbuffer.n_s, src_gbuffer.n_g, src_wo);

		Material src_hit_mat = load_material(src_gbuffer.material_idx, src_gbuffer.uv);
		float src_pdf = bsdf_pdf(src_hit_mat, src_gbuffer.n_s, src_wo, src_wi);

		g_source *= src_pdf;

		if (!same_hemisphere(src_wo, src_wi, src_gbuffer.n_s)) {
			return false;
		}
		jacobian = g_source == 0 ? 0 : jacobian / g_source;
		if (jacobian <= 0.0 || isnan(jacobian) || isinf(jacobian)) {
			jacobian = 0;
			return false;
		}
		// Compute the direct lighting on the reconnection vertex
		const Material rc_mat = load_material(rc_src_gbuffer.material_idx, rc_src_gbuffer.uv);

		if (/*is_diffuse(rc_mat)*/ true) {	// TODO: This causes some bias for some reason.
			reservoir_contribution = data.rc_Li;
		} else {
			const float light_pick_pdf = 1. / pc_ray.light_triangle_count;
			uvec4 reconnection_seed = init_rng(dst_coords, gl_LaunchSizeEXT.xy, seed_helper, data.rc_seed);
			reservoir_contribution =
				uniform_sample_light_with_visibility_override(reconnection_seed, rc_mat, rc_src_gbuffer.pos,
															  rc_side, rc_src_gbuffer.n_s, -dst_wi, rc_nee_visible) /
				light_pick_pdf;
		}

		float rc_cos_x = dot(rc_src_gbuffer.n_s, data.rc_wi);
		float bsdf_pdf;
		const vec3 f = eval_bsdf(rc_src_gbuffer.n_s, -dst_wi, rc_mat, 1, rc_side, data.rc_wi, bsdf_pdf, rc_cos_x);
		if (bsdf_pdf > 0) {
			reservoir_contribution += f * abs(rc_cos_x) * data.rc_postfix_L / bsdf_pdf;
		}
		reservoir_contribution = partial_throughput * reservoir_contribution;
		jacobian_out = jacobian;
		return true;
	}
#endif
	return false;
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