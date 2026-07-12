#ifndef COMMONS_DEVICE
#define COMMONS_DEVICE
#include "commons.h"
#include "utils.glsl"
#include "atmosphere/atmosphere.glsl"

#ifndef SCENE_TEX_IDX
#define SCENE_TEX_IDX 4
#endif

layout(binding = 0, rgba32f) uniform image2D image;
layout(binding = 1) uniform SceneUBOBuffer { SceneUBO ubo; };
layout(binding = 2) buffer SceneDesc_ { SceneDesc scene_desc; };
layout(binding = 3, scalar) readonly buffer Lights { Light lights[]; };
layout(binding = SCENE_TEX_IDX) uniform sampler2D scene_textures[];

layout(set = 1, binding = 0) uniform accelerationStructureEXT tlas;
#include "scene_buffers.glsl"

#include "bsdf_commons.glsl"

vec4 sample_camera(in vec2 ndc) {
	vec4 target = ubo.inv_projection * vec4(ndc.x, ndc.y, 1, 1);
	return ubo.inv_view * vec4(normalize(target.xyz), 0);  // direction
}

vec4 sample_prev_camera(in vec2 ndc) {
	vec4 target = inverse(ubo.prev_projection) * vec4(ndc.x, ndc.y, 1, 1);
	return inverse(ubo.prev_view) * vec4(normalize(target.xyz), 0);	 // direction
}

float uniform_cone_pdf(float cos_max) { return 1. / (TWO_PI * (1 - cos_max)); }

bool is_light_finite(uint light_props) { return (light_props & LIGHT_FLAG_FINITE) != 0; }

bool is_light_delta(uint light_props) { return (light_props & LIGHT_FLAG_DELTA) != 0; }

bool is_light_delta_position(uint light_props) { return (light_props & LIGHT_FLAG_DELTA_POSITION) != 0; }

bool is_light_delta_direction(uint light_props) { return (light_props & LIGHT_FLAG_DELTA_DIRECTION) != 0; }

bool is_light_two_sided(uint light_props) { return (light_props & LIGHT_FLAG_TWO_SIDED) != 0; }

uint get_light_type(uint light_props) { return uint(light_props & LIGHT_TYPE_MASK); }

TriangleRecord triangle_at_bary(PrimInfo pinfo, vec2 uv, uint triangle_idx, in mat4 world_matrix) {
	TriangleRecord result;
	uint index_offset = pinfo.index_offset + 3 * triangle_idx;
	uint vertex_offset = pinfo.vertex_offset;
	ivec3 ind = ivec3(DEREF(index)[index_offset + 0], DEREF(index)[index_offset + 1], DEREF(index)[index_offset + 2]);
	ind += ivec3(vertex_offset);
	Vertex vtx[3];
	vtx[0] = DEREF(compact_vertices)[ind.x];
	vtx[1] = DEREF(compact_vertices)[ind.y];
	vtx[2] = DEREF(compact_vertices)[ind.z];
	const vec3 v0 = vtx[0].pos;
	const vec3 v1 = vtx[1].pos;
	const vec3 v2 = vtx[2].pos;
	const vec3 barycentrics = vec3(1.0 - uv.x - uv.y, uv.x, uv.y);
	const vec3 world_v0 = vec3(world_matrix * vec4(v0, 1.0));
	const vec3 world_v1 = vec3(world_matrix * vec4(v1, 1.0));
	const vec3 world_v2 = vec3(world_matrix * vec4(v2, 1.0));
	const vec3 world_e1 = world_v1 - world_v0;
	const vec3 world_e2 = world_v2 - world_v0;
	const vec3 world_cross = cross(world_e1, world_e2);
	const vec3 object_n_s =
		normalize(vtx[0].normal * barycentrics.x + vtx[1].normal * barycentrics.y + vtx[2].normal * barycentrics.z);

	result.pos = world_v0 * barycentrics.x + world_v1 * barycentrics.y + world_v2 * barycentrics.z;
	result.n_g = normalize(world_cross);
	result.n_s = normalize(vec3(transpose(inverse(world_matrix)) * vec4(object_n_s, 0.0)));
	result.bary = uv;
	result.uv = vtx[0].uv0 * barycentrics.x + vtx[1].uv0 * barycentrics.y + vtx[2].uv0 * barycentrics.z;
	result.triangle_pdf = 2.0 / length(world_cross);
	return result;
}

TriangleRecord sample_triangle(PrimInfo pinfo, vec2 rands, uint triangle_idx, in mat4 world_matrix, out vec2 uv) {
	uv = vec2(1.0 - sqrt(rands.x), rands.y * sqrt(rands.x));
	return triangle_at_bary(pinfo, uv, triangle_idx, world_matrix);
}

TriangleRecord sample_triangle(PrimInfo pinfo, vec2 rands, uint triangle_idx, in mat4 world_matrix) {
	vec2 unused_bary;
	return sample_triangle(pinfo, rands, triangle_idx, world_matrix, unused_bary);
}

vec3 shade_atmosphere(uint dir_light_idx, vec3 sky_col, vec3 ray_origin, vec3 ray_dir, float ray_length) {
	if (dir_light_idx == -1) {
		return sky_col;
	}
	Light light = lights[dir_light_idx];
	vec3 light_dir = -normalize(light.to - light.pos);
	vec3 transmittance;
	vec2 planet_isect = planet_intersection(ray_origin, ray_dir);
	if (planet_isect.x > 0) {
		ray_length = min(ray_length, planet_isect.x);
	}
	return integrate_scattering(ray_origin, ray_dir, ray_length, light_dir, light.L, transmittance);
}
/*
	Light sampling
*/

vec3 uniform_sample_cone(vec2 uv, float cos_max) {
	const float cos_theta = (1. - uv.x) + uv.x * cos_max;
	const float sin_theta = sqrt(1 - cos_theta * cos_theta);
	const float phi = uv.y * TWO_PI;
	return vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
}

LightSampleIdentity invalid_light_identity() {
	LightSampleIdentity identity;
	identity.light_idx = INVALID_LIGHT_INDEX;
	identity.primitive_idx = INVALID_LIGHT_INDEX;
	identity.triangle_idx = INVALID_LIGHT_INDEX;
	identity.bary = vec2(0.0);
	return identity;
}

LightLiSample empty_light_Li_sample() {
	LightLiSample result;
	result.Li = vec3(0.0);
	result.position = vec3(0.0);
	result.normal = vec3(0.0);
	result.wi = vec3(0.0);
	result.distance = 0.0;
	result.selection_pmf = 0.0;
	result.pdf_position_a = 0.0;
	result.pdf_position_w = 0.0;
	result.flags = 0u;
	result.identity = invalid_light_identity();
	return result;
}

LightLeSample empty_light_Le_sample() {
	LightLeSample result;
	result.Le = vec3(0.0);
	result.position = vec3(0.0);
	result.normal = vec3(0.0);
	result.wi = vec3(0.0);
	result.distance = 0.0;
	result.cos_from_light = 0.0;
	result.selection_pmf = 0.0;
	result.pdf_position_a = 0.0;
	result.pdf_direction_w = 0.0;
	result.pdf_joint = 0.0;
	result.flags = 0u;
	result.identity = invalid_light_identity();
	return result;
}

uint light_index_from_primitive(uint primitive_idx) { return DEREF(emitter_light_idx)[primitive_idx]; }

uint sample_area_light_triangle(const Light light, float xi) {
	// Select a triangle uniformly proportional to its area using binary search on the CDF
	const float target_area = xi * light.mesh_area;
	uint first = 0u;
	uint count = light.num_triangles;
	while (count > 0u) {
		const uint step = count >> 1;
		const uint candidate = first + step;
		if (DEREF(light_triangle_cdf)[light.triangle_cdf_offset + candidate].cumulative_area < target_area) {
			first = candidate + 1u;
			count -= step + 1u;
		} else {
			count = step;
		}
	}
	return DEREF(light_triangle_cdf)[light.triangle_cdf_offset + min(first, light.num_triangles - 1u)].triangle_idx;
}

TriangleRecord sample_area_light_by_area(const Light light, vec3 xi, out uint triangle_idx) {
	triangle_idx = sample_area_light_triangle(light, xi.x);
	return sample_triangle(DEREF(prim_info)[light.prim_mesh_idx], xi.yz, triangle_idx, light.world_matrix);
}

float spot_falloff(const Light light, float cos_theta) {
	if (cos_theta < light.cos_outer) {
		return 0.0;
	}
	if (cos_theta >= light.cos_inner || light.cos_inner == light.cos_outer) {
		return 1.0;
	}
	const float t = (cos_theta - light.cos_outer) / (light.cos_inner - light.cos_outer);
	return t * t * t * t;
}

float light_selection_pmf(int num_lights) { return 1.0 / float(num_lights); }

float area_pdf_a_to_w(float pdf_a, float distance_squared, float abs_cos_from_light) {
	return abs_cos_from_light == 0.0 ? 0.0 : pdf_a * distance_squared / abs_cos_from_light;
}

float light_direct_pdf_w(uint light_idx, vec3 reference_position, vec3 light_position, vec3 light_normal,
						 int num_lights) {
	const Light light = lights[light_idx];
	const float selection_pmf = light_selection_pmf(num_lights);
	if (get_light_type(light.light_flags) != LIGHT_AREA) {
		return selection_pmf;
	}
	const vec3 to_light = light_position - reference_position;
	const float distance_squared = dot(to_light, to_light);
	const float abs_cos_from_light = abs(dot(light_normal, -normalize(to_light)));
	return area_pdf_a_to_w(selection_pmf / light.mesh_area, distance_squared, abs_cos_from_light);
}

float light_emission_position_pdf_a(uint light_idx, int num_lights) {
	const Light light = lights[light_idx];
	const float selection_pmf = light_selection_pmf(num_lights);
	switch (get_light_type(light.light_flags)) {
		case LIGHT_AREA:
			return selection_pmf / light.mesh_area;
		case LIGHT_DIRECTIONAL:
			return selection_pmf / (PI * light.world_radius * light.world_radius);
		default:
			return selection_pmf;
	}
}

float light_emission_direction_pdf_w(uint light_idx, vec3 light_normal, vec3 wi) {
	const Light light = lights[light_idx];
	switch (get_light_type(light.light_flags)) {
		case LIGHT_AREA: {
			const float cos_from_light = dot(light_normal, wi);
			return is_light_two_sided(light.light_flags) ? abs(cos_from_light) * (0.5 * INV_PI)
														 : max(cos_from_light, 0.0) * INV_PI;
		}
		case LIGHT_POINT:
			return 0.25 * INV_PI;
		case LIGHT_SPOT:
			return dot(normalize(light.to - light.pos), wi) >= light.cos_outer ? uniform_cone_pdf(light.cos_outer)
																			   : 0.0;
		case LIGHT_DIRECTIONAL:
			return 1.0;
	}
	return 0.0;
}

float light_emission_pdf(uint light_idx, vec3 light_normal, vec3 wi, int num_lights) {
	return light_emission_position_pdf_a(light_idx, num_lights) *
		   light_emission_direction_pdf_w(light_idx, light_normal, wi);
}

LightLiSample replay_light_Li(LightSampleIdentity identity, vec3 p, int num_lights) {
	LightLiSample result = empty_light_Li_sample();
	if (num_lights == 0 || identity.light_idx == INVALID_LIGHT_INDEX) {
		return result;
	}

	const uint light_idx = identity.light_idx;
	const Light light = lights[light_idx];
	const uint light_type = get_light_type(light.light_flags);
	result.selection_pmf = light_selection_pmf(num_lights);
	result.flags = light.light_flags;
	result.identity = identity;

	switch (light_type) {
		case LIGHT_AREA: {
			const TriangleRecord record = triangle_at_bary(DEREF(prim_info)[light.prim_mesh_idx], identity.bary,
														   identity.triangle_idx, light.world_matrix);
			const vec3 to_light = record.pos - p;
			const float distance_squared = dot(to_light, to_light);
			result.distance = sqrt(distance_squared);
			result.wi = to_light / result.distance;
			result.position = record.pos;
			result.normal = record.n_g;
			result.pdf_position_a = result.selection_pmf / light.mesh_area;
			const float cos_from_light = dot(record.n_g, -result.wi);
			result.pdf_position_w = area_pdf_a_to_w(result.pdf_position_a, distance_squared, abs(cos_from_light));
			if (is_light_two_sided(light.light_flags) || cos_from_light > 0.0) {
				result.Li = load_material(light.material_idx, record.uv).emissive_factor;
			}
		} break;
		case LIGHT_POINT:
		case LIGHT_SPOT: {
			const vec3 to_light = light.pos - p;
			const float distance_squared = dot(to_light, to_light);
			result.distance = sqrt(distance_squared);
			result.wi = to_light / result.distance;
			result.position = light.pos;
			result.normal = -result.wi;
			result.pdf_position_a = result.selection_pmf;
			result.pdf_position_w = result.selection_pmf;
			float falloff = 1.0;
			if (light_type == LIGHT_SPOT) {
				falloff = spot_falloff(light, dot(-result.wi, normalize(light.to - light.pos)));
			}
			result.Li = light.L * (falloff / distance_squared);

		} break;
		case LIGHT_DIRECTIONAL: {
			result.wi = normalize(light.pos - light.to);
			result.distance = 2.0 * light.world_radius;
			result.position = p + result.wi * result.distance;
			result.normal = -result.wi;
			result.Li = light.L;
			result.pdf_position_a = result.selection_pmf;
			result.pdf_position_w = result.selection_pmf;
		} break;
	}
	return result;
}

// Samples a light object uniformly
LightLiSample sample_light_Li(vec4 xi, vec3 p, int num_lights) {
	if (num_lights == 0) {
		return empty_light_Li_sample();
	}

	LightSampleIdentity identity = invalid_light_identity();
	identity.light_idx = min(uint(xi.x * num_lights), uint(num_lights - 1));
	const Light light = lights[identity.light_idx];
	if (get_light_type(light.light_flags) == LIGHT_AREA) {
		identity.primitive_idx = light.prim_mesh_idx;
		identity.triangle_idx = sample_area_light_triangle(light, xi.y);
		const float sqrt_x = sqrt(xi.z);
		identity.bary = vec2(1.0 - sqrt_x, xi.w * sqrt_x);
	}
	return replay_light_Li(identity, p, num_lights);
}

// Samples emitted rays.
// pdf_position_a includes light selection
// pdf_direction_w is conditional on that endpoint.
// pdf_joint is their product.
LightLeSample sample_light_Le(vec4 xi_position, vec2 xi_direction, int num_lights) {
	LightLeSample result = empty_light_Le_sample();
	if (num_lights == 0) {
		return result;
	}

	const uint light_idx = min(uint(xi_position.x * num_lights), uint(num_lights - 1));
	const Light light = lights[light_idx];
	const uint light_type = get_light_type(light.light_flags);
	result.selection_pmf = light_selection_pmf(num_lights);
	result.flags = light.light_flags;
	result.identity.light_idx = light_idx;

	switch (light_type) {
		case LIGHT_AREA: {
			uint triangle_idx;
			const TriangleRecord record = sample_area_light_by_area(light, xi_position.yzw, triangle_idx);
			result.position = record.pos;
			result.normal = record.n_g;
			result.identity.primitive_idx = light.prim_mesh_idx;
			result.identity.triangle_idx = triangle_idx;
			result.identity.bary = record.bary;
			result.pdf_position_a = result.selection_pmf / light.mesh_area;
			result.Le = load_material(light.material_idx, record.uv).emissive_factor;

			if (is_light_two_sided(light.light_flags)) {
				const bool back_side = xi_direction.x >= 0.5;
				const vec2 hemisphere_xi = vec2(fract(2.0 * xi_direction.x), xi_direction.y);
				result.wi = sample_hemisphere(hemisphere_xi, back_side ? -record.n_g : record.n_g);
				result.cos_from_light = abs(dot(record.n_g, result.wi));
				result.pdf_direction_w = result.cos_from_light * (0.5 * INV_PI);
			} else {
				result.wi = sample_hemisphere(xi_direction, record.n_g);
				result.cos_from_light = max(dot(record.n_g, result.wi), 0.0);
				result.pdf_direction_w = result.cos_from_light * INV_PI;
			}
		} break;
		case LIGHT_POINT: {
			const float z = 1.0 - 2.0 * xi_direction.x;
			const float radial = sqrt(max(0.0, 1.0 - z * z));
			const float phi = TWO_PI * xi_direction.y;
			result.position = light.pos;
			result.wi = vec3(radial * cos(phi), radial * sin(phi), z);
			result.normal = result.wi;
			result.Le = light.L;
			result.cos_from_light = 1.0;
			result.pdf_position_a = result.selection_pmf;
			result.pdf_direction_w = 0.25 * INV_PI;
		} break;
		case LIGHT_SPOT: {
			const vec3 axis = normalize(light.to - light.pos);
			vec3 tangent;
			vec3 bitangent;
			make_coord_system(axis, tangent, bitangent);
			const vec3 local_wi = uniform_sample_cone(xi_direction, light.cos_outer);
			result.position = light.pos;
			result.wi = local_wi.x * tangent + local_wi.y * bitangent + local_wi.z * axis;
			result.normal = axis;
			result.cos_from_light = dot(result.wi, axis);
			result.Le = light.L * spot_falloff(light, result.cos_from_light);
			result.pdf_position_a = result.selection_pmf;
			result.pdf_direction_w = uniform_cone_pdf(light.cos_outer);
		} break;
		case LIGHT_DIRECTIONAL: {
			const vec3 emission_direction = normalize(light.to - light.pos);
			vec3 tangent;
			vec3 bitangent;
			make_coord_system(emission_direction, tangent, bitangent);
			const vec2 disk = concentric_sample_disk(xi_position.yz);
			result.position = light.world_center - emission_direction * light.world_radius +
							  light.world_radius * (disk.x * tangent + disk.y * bitangent);
			result.normal = emission_direction;
			result.wi = emission_direction;
			result.Le = light.L;
			result.cos_from_light = 1.0;
			result.pdf_position_a = result.selection_pmf / (PI * light.world_radius * light.world_radius);
			result.pdf_direction_w = 1.0;
		} break;
	}
	result.pdf_joint = result.pdf_position_a * result.pdf_direction_w;
	return result;
}

#endif
