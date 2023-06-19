#include "../../../commons.glsl"
#include "gris_commons.h"
layout(push_constant) uniform _PushConstantRay { PCReSTIRPT pc_ray; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GrisReservoir { Reservoir d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GrisGBuffer { GBuffer d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GrisDirectLighting { vec3 d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer PrefixContributions { vec3 d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer PathReconnections { ReconnectionData d[]; };
const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;
#define RR_MIN_DEPTH 3
uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);
uvec4 seed = init_rng(gl_LaunchIDEXT.xy, gl_LaunchSizeEXT.xy, pc_ray.total_frame_num ^ pc_ray.random_num);

#include "../../pt_commons.glsl"

void init_reservoir(out Reservoir r) {
	r.M = 0;
	r.W = 0.0;
	r.w_sum = 0.0;
}
void init_data(out GrisData data) {
	data.rc_mat_id = -1;
	data.rc_postfix_L = vec3(0);
	data.rc_g = 0;
	data.reservoir_contribution = vec3(0);
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

void calc_reservoir_W(inout Reservoir r, float target_pdf) {
	r.W = target_pdf == 0.0 ? 0.0 : r.w_sum / target_pdf;
}

float calc_target_pdf(vec3 f) {
	return luminance(f);
}

vec3 calc_reservoir_contribution(GrisData data, vec3 L_direct, vec3 wo, vec3 partial_throughput) {
	const Material rc_mat = load_material(data.rc_mat_id, data.rc_uv);
	float cos_x = dot(data.rc_ns, data.rc_wi);
	float bsdf_pdf;
	vec3 result = L_direct;
	bool rc_side = (data.path_flags & 0x1F) == 1;
	const vec3 f = eval_bsdf(data.rc_ns, wo, rc_mat, 1, rc_side, data.rc_wi, bsdf_pdf, cos_x);
	if(bsdf_pdf > 0) {
		result += f * abs(cos_x) * data.rc_postfix_L / bsdf_pdf;
	}
	return partial_throughput * result;
}

vec3 calc_reservoir_integrand(in Reservoir r) {
	if(r.W > 0) {
		return r.data.reservoir_contribution * r.W;
	}
	return vec3(0);
}

void init_gbuffer(out GBuffer gbuffer) { gbuffer.material_idx = -1; }

bool is_rough(in Material mat) {
	// Only check if it's diffuse for now
	return (mat.bsdf_type & BSDF_DIFFUSE) != 0;
}

bool is_diffuse(in Material mat) { return (mat.bsdf_type & BSDF_DIFFUSE) != 0; }

uint offset(const uint pingpong) {
    return pingpong * pc_ray.size_x * pc_ray.size_y;
}

uint set_path_flags(bool side, bool nee_visible, uint prefix_length) {
	return  (prefix_length & 0x1F) << 2 | uint(nee_visible) << 1 | uint(side);
}

void unpack_path_flags(uint packed_data, out bool side, out bool nee_visible, out uint prefix_length, out uint postfix_length) {
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

bool retrace_paths(in GBuffer gbuffer, in GrisData data, uvec2 source_coords, uvec2 target_coords,
				   out vec3 prefix_contribution, out float jacobian, out vec3 reservoir_contribution) {
	if (data.rc_mat_id == -1) {
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

	prefix_contribution = vec3(0);

	uint prefix_depth = 0;
	bool specular = false;

	vec3 n_s = gbuffer.n_s;
	vec3 n_g = gbuffer.n_g;
	vec3 pos = gbuffer.pos;

	uvec4 reservoir_seed =
		init_rng(target_coords, gl_LaunchSizeEXT.xy, pc_ray.total_frame_num ^ pc_ray.random_num, data.init_seed);

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

		vec3 wi = data.rc_pos - pos;
		float wi_len = length(wi);
		wi /= wi_len;

		if (prefix_depth > 0 && (hit_mat.bsdf_props & BSDF_SPECULAR) == 0) {
			const float light_pick_pdf = 1. / pc_ray.light_triangle_count;
			vec3 contrib = prefix_throughput *
						   uniform_sample_light(reservoir_seed, hit_mat, payload.pos, side, n_s, wo) / light_pick_pdf;
			prefix_contribution += contrib;
		}

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
			float g_curr = abs(dot(data.rc_ns, wi)) / (wi_len * wi_len);
			float j = data.rc_g == 0.0 ? 0.0 : g_curr / data.rc_g;
			jacobian = j;
			// Compute the direct lighting on the reconnection vertex
			const Material rc_mat = load_material(data.rc_mat_id, data.rc_uv);

			vec3 L_direct;
			if (is_diffuse(rc_mat)) {
				L_direct = data.rc_Li;
			} else {
				const float light_pick_pdf = 1. / pc_ray.light_triangle_count;
				uvec4 reconnection_seed = init_rng(target_coords, gl_LaunchSizeEXT.xy,
												   pc_ray.total_frame_num ^ pc_ray.random_num, data.rc_seed);
				L_direct = uniform_sample_light_with_visibility_override(reconnection_seed, rc_mat, data.rc_pos,
																		 rc_side, data.rc_ns, -wi, rc_nee_visible) /
						   light_pick_pdf;
			}
			

			float rc_cos_x = dot(data.rc_ns, data.rc_wi);
			float bsdf_pdf;
			reservoir_contribution = L_direct;
			const vec3 f = eval_bsdf(data.rc_ns, wo, rc_mat, 1, rc_side, data.rc_wi, bsdf_pdf, rc_cos_x);
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
		specular = (hit_mat.bsdf_props & BSDF_SPECULAR) != 0;

		traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, origin, tmin, direction, tmax, 0);
		const bool found_isect = payload.material_idx != -1;

		if (!found_isect) {
			return false;
		}
		hit_mat = load_material(payload.material_idx, payload.uv);

		if (specular) {
			prefix_contribution += prefix_throughput * hit_mat.emissive_factor;
		}

		n_s = payload.n_s;
		n_g = payload.n_g;
		pos = payload.pos;

		prefix_depth++;
	}

	return false;
}

bool retrace_paths_and_evaluate(in GBuffer gbuffer, in GrisData data, uvec2 source_coords, uvec2 target_coords,
								out float target_pdf) {
	vec3 unused;
	vec3 reservoir_contribution;
	float jacobian;
	bool result = retrace_paths(gbuffer, data, source_coords, target_coords, unused, jacobian, reservoir_contribution);
	target_pdf = calc_target_pdf(reservoir_contribution) * jacobian;
	return result;
}