#include "../../../bda.glsl"
#include "gris_commons.h"
#include "../../../commons.glsl"
#include "../../../surface.glsl"
layout(location = 0) rayPayloadEXT SurfaceHitPayload payload;
layout(location = 1) rayPayloadEXT AnyHitPayload any_hit_payload;
layout(push_constant) uniform _PushConstantRay { PCReSTIRPT pc; };

#define LOG_GRIS 0

const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;
uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);

ivec2 get_neighbor_offset(inout uvec4 seed) {
	const float randa = rand(seed) * 2 * PI;
	const float randr = sqrt(rand(seed)) * pc.spatial_radius;
	return ivec2(floor(cos(randa) * randr), floor(sin(randa) * randr));
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

float calc_target_pdf(vec3 f) { return luminance(f); }

bool is_rough(in Material mat) {
	// Only check if it's diffuse for now
	return (mat.bsdf_type & BSDF_TYPE_DIFFUSE) != 0 || mat.roughness > 0.25;
}

uint pack_photon_flags(bool side, uint path_length) { return (path_length & 0x1F) << 1 | uint(side); }

void unpack_photon_flags(uint flags, out bool side, out uint path_length) {
	side = (flags & 1) == 1;
	path_length = (flags >> 1) & 0x1F;
}

bool photon_is_nearby(vec3 p, vec3 photon_p, float r) {
	vec3 diff = p - photon_p;
	return dot(diff, diff) < r * r;
}

bool update_photon_reservoir(inout uvec4 seed, inout PhotonReservoir r_new, vec3 flux, SurfaceRef surface, vec2 wi,
							 float d_vm, float target_pdf, float inv_source_pdf) {
	float w_i = target_pdf * inv_source_pdf;
	r_new.w_sum += w_i;
	if (rand(seed) * r_new.w_sum < w_i) {
		r_new.flux = flux;
		r_new.surface = surface;
		r_new.wi = wi;
		r_new.target_pdf = target_pdf;
		r_new.d_vm = d_vm;
		return true;
	}
	return false;
}

void init_reservoir(out PhotonReservoir r) {
	r.flux = vec3(0);
	r.M = 0;
	r.W = 0;
	r.w_sum = 0;
	r.target_pdf = 0;
	r.d_vm = 0;
	r.surface = invalid_surface_ref();
}

bool photon_data_valid(PhotonData photon) { return surface_ref_valid(photon.surface); }

bool photon_reservoir_data_valid(PhotonReservoir reservoir) { return surface_ref_valid(reservoir.surface); }

bool combine_reservoir(inout uvec4 seed, inout PhotonReservoir target_reservoir, const PhotonReservoir input_reservoir,
					   float target_pdf, float mis_weight) {
	target_reservoir.M += input_reservoir.M;
	float inv_source_pdf = input_reservoir.W * mis_weight;
	if (target_pdf <= 0.0) {
		return false;
	}
	return update_photon_reservoir(seed, target_reservoir, input_reservoir.flux, input_reservoir.surface,
								   input_reservoir.wi, input_reservoir.d_vm, target_pdf, inv_source_pdf);
}

bool stream_photon_reservoir(inout uvec4 seed, inout PhotonReservoir r_new, vec3 flux, SurfaceRef surface, vec2 wi,
							 float d_vm, float target_pdf, float inv_source_pdf) {
	r_new.M++;
	if (target_pdf <= 0.0 || isnan(inv_source_pdf) || inv_source_pdf <= 0.0) {
		return false;
	}
	return update_photon_reservoir(seed, r_new, flux, surface, wi, d_vm, target_pdf, inv_source_pdf);
}

bool is_photon_valid(SurfaceData cam_gbuffer, SurfaceData photon_gbuffer, vec3 wi, uint eye_path_length,
					 uint photon_path_length) {
	// Check 1
	if (!photon_is_nearby(cam_gbuffer.pos, photon_gbuffer.pos, pc.photon_radius)) {
		return false;
	}
	// Check 2
	if (pc.max_depth < (eye_path_length + photon_path_length)) {
		return false;
	}
	// // Check 3
	if (dot(photon_gbuffer.n_s, cam_gbuffer.n_s) < 0.5) {
		return false;
	}
	// // Check 4
	if (dot(cam_gbuffer.n_s, wi) <= 0) {
		return false;
	}
	return true;
}

void calc_photon_reservoir_W_with_mis(inout PhotonReservoir r) {
	r.W = r.target_pdf == 0.0 ? 0.0 : r.w_sum / r.target_pdf;
}
