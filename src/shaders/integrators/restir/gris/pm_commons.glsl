#include "../../../bda.glsl"
#include "gris_commons.h"
#include "../../../commons.glsl"
layout(location = 0) rayPayloadEXT GrisHitPayload payload;
layout(location = 1) rayPayloadEXT AnyHitPayload any_hit_payload;
layout(push_constant) uniform _PushConstantRay { PCReSTIRPT pc; };

#define LOG_GRIS 0

SCENE_BUFFER_RO(transformations, mat4);
const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;
uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);

struct HitData {
	vec3 pos;
	vec3 n_g;
	vec3 n_s;
	vec2 uv;
	uint material_idx;
};

ivec2 get_neighbor_offset(inout uvec4 seed) {
	const float randa = rand(seed) * 2 * PI;
	const float randr = sqrt(rand(seed)) * pc.spatial_radius;
	return ivec2(floor(cos(randa) * randr), floor(sin(randa) * randr));
}

HitData get_hitdata(vec2 attribs, uint instance_idx, uint triangle_idx, out float area) {
	const PrimInfo pinfo = DEREF(prim_info)[instance_idx];
	const uint index_offset = pinfo.index_offset + 3 * triangle_idx;
	const ivec3 ind = ivec3(pinfo.vertex_offset) +
					  ivec3(DEREF(index)[index_offset + 0], DEREF(index)[index_offset + 1], DEREF(index)[index_offset + 2]);
	const vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
	const mat4 to_world = DEREF(transformations)[instance_idx];
	const mat4 tsp_inv_to_world = transpose(inverse(to_world));

	HitData gbuffer;
	Vertex vtx[3];

	vtx[0] = DEREF(compact_vertices)[ind.x];
	vtx[1] = DEREF(compact_vertices)[ind.y];
	vtx[2] = DEREF(compact_vertices)[ind.z];

	gbuffer.pos = vec3(to_world * vec4(vtx[0].pos * bary.x + vtx[1].pos * bary.y + vtx[2].pos * bary.z, 1.0));
	gbuffer.n_s = normalize(
		vec3(tsp_inv_to_world * vec4(vtx[0].normal * bary.x + vtx[1].normal * bary.y + vtx[2].normal * bary.z, 1.0)));
	const vec3 e0 = vtx[2].pos - vtx[0].pos;
	const vec3 e1 = vtx[1].pos - vtx[0].pos;

	const vec4 e0t = to_world * vec4(vtx[2].pos - vtx[0].pos, 0);
	const vec4 e1t = to_world * vec4(vtx[1].pos - vtx[0].pos, 0);
	area = 0.5 * length(cross(vec3(e0t), vec3(e1t)));
	gbuffer.n_g = normalize((tsp_inv_to_world * vec4(cross(e0, e1), 0)).xyz);
	gbuffer.uv = vtx[0].uv0 * bary.x + vtx[1].uv0 * bary.y + vtx[2].uv0 * bary.z;
	gbuffer.material_idx = pinfo.material_index;
	return gbuffer;
}

HitData get_hitdata(vec2 attribs, uint instance_idx, uint triangle_idx) {
	float unused;
	return get_hitdata(attribs, instance_idx, triangle_idx, unused);
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

bool update_photon_reservoir(inout uvec4 seed, inout PhotonReservoir r_new, vec3 flux, vec2 barycentrics,
							 uvec2 primitive_instance_id, vec2 wi, float d_vm, float target_pdf, float inv_source_pdf) {
	float w_i = target_pdf * inv_source_pdf;
	r_new.w_sum += w_i;
	if (rand(seed) * r_new.w_sum < w_i) {
		r_new.flux = flux;
		r_new.barycentrics = barycentrics;
		r_new.primitive_instance_id = primitive_instance_id;
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
	r.primitive_instance_id = uvec2(-1);
}

bool photon_data_valid(PhotonData photon) { return photon.primitive_instance_id.x != -1; }

bool photon_reservoir_data_valid(PhotonReservoir reservoir) { return reservoir.primitive_instance_id.x != -1; }

bool combine_reservoir(inout uvec4 seed, inout PhotonReservoir target_reservoir, const PhotonReservoir input_reservoir,
					   float target_pdf, float mis_weight) {
	target_reservoir.M += input_reservoir.M;
	float inv_source_pdf = input_reservoir.W * mis_weight;
	if (target_pdf <= 0.0) {
		return false;
	}
	return update_photon_reservoir(seed, target_reservoir, input_reservoir.flux, input_reservoir.barycentrics,
								   input_reservoir.primitive_instance_id, input_reservoir.wi, input_reservoir.d_vm,
								   target_pdf, inv_source_pdf);
}

bool stream_photon_reservoir(inout uvec4 seed, inout PhotonReservoir r_new, vec3 flux, vec2 barycentrics,
							 uvec2 primitive_instance_id, vec2 wi, float d_vm, float target_pdf, float inv_source_pdf) {
	r_new.M++;
	if (target_pdf <= 0.0 || isnan(inv_source_pdf) || inv_source_pdf <= 0.0) {
		return false;
	}
	return update_photon_reservoir(seed, r_new, flux, barycentrics, primitive_instance_id, wi, d_vm, target_pdf,
								   inv_source_pdf);
}

bool is_photon_valid(HitData cam_gbuffer, HitData photon_gbuffer, vec3 wi, uint eye_path_length,
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
