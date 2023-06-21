#include "../../../commons.glsl"
#include "gris_commons.h"
layout(push_constant) uniform _PushConstantRay { PCReSTIRPT pc_ray; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GrisReservoir { Reservoir d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GrisGBuffer { GBuffer d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GrisDirectLighting { vec3 d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer PrefixContributions { vec3 d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer PathReconnections { ReconnectionData d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Transformation { mat4 m[]; };

Transformation transforms = Transformation(scene_desc.transformations_addr);
const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;
#define RR_MIN_DEPTH 3
uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);
uvec4 seed = init_rng(gl_LaunchIDEXT.xy, gl_LaunchSizeEXT.xy, pc_ray.total_frame_num ^ pc_ray.random_num);

#include "../../pt_commons.glsl"

struct UnpackedGBuffer {
	vec3 pos;
	vec3 n_g;
	vec3 n_s;
	vec2 uv;
	uint material_idx;
};

vec3 get_pos(ivec3 ind, vec3 bary, mat4 to_world) {
	const vec3 v0 = vertices.v[ind.x];
	const vec3 v1 = vertices.v[ind.y];
	const vec3 v2 = vertices.v[ind.z];
	const vec3 pos = v0 * bary.x + v1 * bary.y + v2 * bary.z;
	return vec3(to_world * vec4(pos, 1.0));
}

vec3 get_normal(ivec3 ind, vec3 bary, mat4 tsp_inv_to_world) {
	const vec3 n0 = normals.n[ind.x];
	const vec3 n1 = normals.n[ind.y];
	const vec3 n2 = normals.n[ind.z];
	const vec3 n = normalize(n0 * bary.x + n1 * bary.y + n2 * bary.z);
	return normalize(vec3(tsp_inv_to_world * vec4(n, 0.0)));
}

vec2 get_uv(ivec3 ind, vec3 bary) {
	const vec2 uv0 = tex_coords.t[ind.x];
	const vec2 uv1 = tex_coords.t[ind.y];
	const vec2 uv2 = tex_coords.t[ind.z];
	return uv0 * bary.x + uv1 * bary.y + uv2 * bary.z;
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

void unpack_gbuffer(vec2 barycentrics, uvec2 primitive_instance_id, out UnpackedGBuffer unpacked_gbuffer) {
	const PrimMeshInfo pinfo = prim_infos.d[primitive_instance_id.y];
	const uint index_offset = pinfo.index_offset + 3 * primitive_instance_id.x;
	const ivec3 ind = ivec3(indices.i[index_offset + 0], indices.i[index_offset + 1], indices.i[index_offset + 2]);
	const vec3 bary = vec3(1.0 - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);
	const mat4 to_world = transforms.m[primitive_instance_id.y];
	unpacked_gbuffer.pos = get_pos(ind, bary, to_world);
	unpacked_gbuffer.n_s = get_normal(ind, bary, transpose(inverse(to_world)));
	unpacked_gbuffer.uv = get_uv(ind, bary);
	unpacked_gbuffer.material_idx = pinfo.material_index;
}

void unpack_gbuffer(in GBuffer gbuffer, out UnpackedGBuffer unpacked_gbuffer) {
	const PrimMeshInfo pinfo = prim_infos.d[gbuffer.primitive_instance_id.y];
	const uint index_offset = pinfo.index_offset + 3 * gbuffer.primitive_instance_id.x;
	const ivec3 ind = ivec3(indices.i[index_offset + 0], indices.i[index_offset + 1], indices.i[index_offset + 2]);
	const vec3 bary = vec3(1.0 - gbuffer.barycentrics.x - gbuffer.barycentrics.y, gbuffer.barycentrics.x, gbuffer.barycentrics.y);
	const mat4 to_world = transforms.m[gbuffer.primitive_instance_id.y];
	const mat4 tsp_inv_to_world = transpose(inverse(to_world));
	
	const vec3 v0 = vertices.v[ind.x];
	const vec3 v1 = vertices.v[ind.y];
	const vec3 v2 = vertices.v[ind.z];
	unpacked_gbuffer.n_s = get_normal(ind, bary, tsp_inv_to_world);
	unpacked_gbuffer.uv = get_uv(ind, bary);
	unpacked_gbuffer.material_idx = pinfo.material_index;
	const vec3 pos = v0 * bary.x + v1 * bary.y + v2 * bary.z;
	unpacked_gbuffer.pos = vec3(to_world * vec4(pos, 1.0));
	const vec3 e0 = v2 - v0;
    const vec3 e1 = v1 - v0;
	unpacked_gbuffer.n_g = (tsp_inv_to_world * vec4(cross(e0, e1), 0)).xyz;
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


vec3 uniform_sample_light_with_visibility_override(inout uvec4 seed, const Material mat, vec3 pos, const bool side,
												   const vec3 n_s, const vec3 wo, bool visible_light) {
	vec3 res = vec3(0);
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
	float bsdf_pdf;
	float cos_x = dot(n_s, wi);
	vec3 f = eval_bsdf(n_s, wo, mat, 1, side, wi, bsdf_pdf, cos_x);
	if (visible_light && pdf_light_w > 0) {
		float mis_weight;
#ifdef MIS
		mis_weight = 1;
#else
		mis_weight = is_light_delta(record.flags) ? 1 : 1 / (1 + bsdf_pdf / pdf_light_w);
#endif
		res += mis_weight * f * abs(cos_x) * Le / pdf_light_w;
	}
#ifdef MIS
	if (get_light_type(record.flags) == LIGHT_AREA) {
		// Sample BSDF
		f = sample_bsdf(n_s, wo, mat, 1, side, wi, bsdf_pdf, cos_x, seed);
		if (bsdf_pdf != 0) {
			traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, p, tmin, wi, tmax, 0);
			if (payload.material_idx == record.material_idx && payload.triangle_idx == record.triangle_idx) {
				const float wi_len = length(payload.pos - pos);
				const float g = abs(dot(payload.n_s, -wi)) / (wi_len * wi_len);
				const float mis_weight = 1. / (1 + pdf_light_a / (g * bsdf_pdf));
				res += f * mis_weight * abs(cos_x) * Le / bsdf_pdf;
			}
		}
	}
#endif
	return res;
}

bool retrace_paths(in UnpackedGBuffer gbuffer, in GrisData data, uvec2 source_coords, uvec2 target_coords,
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
		init_rng(target_coords, gl_LaunchSizeEXT.xy, pc_ray.total_frame_num ^ pc_ray.random_num, data.init_seed);

	UnpackedGBuffer rc_gbuffer;
	unpack_gbuffer(data.rc_barycentrics, data.rc_primitive_instance_id, rc_gbuffer);
	if(!rc_side) {
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
			vec3 partial_throughput = prefix_throughput * rc_f * abs(cos_x);
			// Compute the Jacobian
			float g_curr = abs(dot(rc_gbuffer.n_s, wi)) / (wi_len * wi_len);
			float j = data.rc_g == 0.0 ? 0.0 : g_curr / data.rc_g;
			jacobian = j;
			// Compute the direct lighting on the reconnection vertex
			const Material rc_mat = load_material(rc_gbuffer.material_idx, rc_gbuffer.uv);

			vec3 L_direct;
			if (is_diffuse(rc_mat)) {
				L_direct = data.rc_Li;
			} else {
				const float light_pick_pdf = 1. / pc_ray.light_triangle_count;
				uvec4 reconnection_seed = init_rng(target_coords, gl_LaunchSizeEXT.xy,
												   pc_ray.total_frame_num ^ pc_ray.random_num, data.rc_seed);
				L_direct = uniform_sample_light_with_visibility_override(reconnection_seed, rc_mat, rc_gbuffer.pos, rc_side,
																		 rc_gbuffer.n_s, -wi, rc_nee_visible) /
						   light_pick_pdf;
			}

			float rc_cos_x = dot(rc_gbuffer.n_s, data.rc_wi);
			float bsdf_pdf;
			reservoir_contribution = L_direct;
			const vec3 f = eval_bsdf(rc_gbuffer.n_s, wo, rc_mat, 1, rc_side, data.rc_wi, bsdf_pdf, rc_cos_x);
			if (bsdf_pdf > 0) {
				reservoir_contribution += f * abs(rc_cos_x) * data.rc_postfix_L / bsdf_pdf;
			}
			reservoir_contribution = partial_throughput * reservoir_contribution;
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
		const bool found_isect = payload.material_idx != -1;

		if (!found_isect) {
			return false;
		}
		hit_mat = load_material(payload.material_idx, payload.uv);

		n_s = payload.n_s;
		n_g = payload.n_g;
		pos = payload.pos;

		prefix_depth++;
	}

	return false;
}

bool retrace_paths_and_evaluate(in UnpackedGBuffer gbuffer, in GrisData data, uvec2 source_coords, uvec2 target_coords,
								out float target_pdf) {
	vec3 reservoir_contribution;
	float jacobian;
	bool result = retrace_paths(gbuffer, data, source_coords, target_coords, jacobian, reservoir_contribution);
	target_pdf = calc_target_pdf(reservoir_contribution) * jacobian;
	return result;
}