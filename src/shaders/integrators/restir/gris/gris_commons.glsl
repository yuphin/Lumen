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
	data.rc_mat_id = -1;
	data.rc_postfix_L = vec3(0);
}

void update_reservoir(inout Reservoir r_new, const GrisData data, float w_i) {
	r_new.w_sum += w_i;
	r_new.M++;
	if (rand(seed) < w_i / r_new.w_sum) {
		r_new.gris_data = data;
	}
}

void init_gbuffer(out GBuffer gbuffer) { gbuffer.material_idx = -1; }

bool is_rough(in Material mat) {
	// Only check if it's diffuse for now
	return (mat.bsdf_type & BSDF_DIFFUSE) != 0;
}

bool is_diffuse(in Material mat) { return (mat.bsdf_type & BSDF_DIFFUSE) != 0; }