#include "../../../commons.glsl"
#include "gris_commons.h"
layout(push_constant) uniform _PushConstantRay { PCReSTIRPT pc_ray; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GrisReservoir { Reservoir d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GrisGBuffer { GBuffer d[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) buffer GrisDirectLighting { vec3 d[]; };
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
void init_gris_data(out GrisData data) {
	data.F = vec3 (0);
	data.postfix_length = 0;
	data.prefix_length = 0;
	data.rc_mat_id = -1;
	data.rc_postfix_L = vec3(0);
	data.rc_nee_L = vec3(0);
	data.rc_g = 0;
	data.reservoir_contribution = vec3(0);
	data.prefix_contribution = vec3(0);
}

bool update_reservoir(inout uvec4 seed, inout Reservoir r_new, const GrisData data, float w_i) {
	r_new.w_sum += w_i;
	r_new.M++;
	if (rand(seed) * r_new.w_sum < w_i) {
		r_new.gris_data = data;
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
	return update_reservoir(seed, target_reservoir, input_reservoir.gris_data, weight);
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
	const vec3 f = eval_bsdf(data.rc_ns, wo, rc_mat, 1, data.rc_side == 1, data.rc_wi, bsdf_pdf, cos_x);
	if(bsdf_pdf > 0) {
		result += f * abs(cos_x) * data.rc_postfix_L / bsdf_pdf;
	}
	return partial_throughput * result;
}

void calc_reservoir_integrand(inout Reservoir r) {
	r.gris_data.F = r.gris_data.prefix_contribution;
	if(r.W > 0) {
		r.gris_data.F += r.gris_data.reservoir_contribution * r.W;
	}
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