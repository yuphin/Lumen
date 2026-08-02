#ifndef SURFACE_GLSL
#define SURFACE_GLSL

#include "scene_buffers.glsl"

struct SurfaceData {
	vec3 pos;
	vec3 n_g;
	vec3 n_s;
	vec2 uv;
	uint material_idx;
	float area;
};

SurfaceRef invalid_surface_ref() {
	SurfaceRef ref;
	ref.barycentrics = vec2(0.0);
	ref.primitive_instance_id = uvec2(INVALID_SURFACE_ID);
	return ref;
}

bool surface_ref_valid(const SurfaceRef ref) {
	return all(notEqual(ref.primitive_instance_id, uvec2(INVALID_SURFACE_ID)));
}

vec3 transform_surface_direction(const InstanceTransform transform, const vec3 direction) {
	return transform.object_to_world_x * direction.x + transform.object_to_world_y * direction.y +
		   transform.object_to_world_z * direction.z;
}

vec3 transform_surface_point(const InstanceTransform transform, const vec3 point) {
	return transform_surface_direction(transform, point) + transform.object_to_world_translation;
}

vec3 transform_surface_normal(const InstanceTransform transform, const vec3 normal) {
	return transform.normal_to_world_x * normal.x + transform.normal_to_world_y * normal.y +
		   transform.normal_to_world_z * normal.z;
}

SurfaceData load_surface(const SurfaceRef ref) {
	const uint triangle_idx = ref.primitive_instance_id.x;
	const uint instance_idx = ref.primitive_instance_id.y;
	const PrimInfo pinfo = DEREF(prim_info)[instance_idx];
	const uint index_offset = pinfo.index_offset + 3 * triangle_idx;
	const ivec3 indices = ivec3(pinfo.vertex_offset) +
		ivec3(DEREF(index)[index_offset], DEREF(index)[index_offset + 1], DEREF(index)[index_offset + 2]);
	const Vertex v0 = DEREF(compact_vertices)[indices.x];
	const Vertex v1 = DEREF(compact_vertices)[indices.y];
	const Vertex v2 = DEREF(compact_vertices)[indices.z];
	const vec3 bary = vec3(1.0 - ref.barycentrics.x - ref.barycentrics.y, ref.barycentrics);
	const InstanceTransform transform = DEREF(transformations)[instance_idx];

	const vec3 object_pos = v0.pos * bary.x + v1.pos * bary.y + v2.pos * bary.z;
	const vec3 world_e0 = transform_surface_direction(transform, v2.pos - v0.pos);
	const vec3 world_e1 = transform_surface_direction(transform, v1.pos - v0.pos);
	const vec3 world_cross = cross(world_e0, world_e1);

	SurfaceData surface;
	surface.pos = transform_surface_point(transform, object_pos);
	surface.n_g = normalize(world_cross);
	surface.n_s = normalize(transform_surface_normal(
		transform, v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z));
	surface.uv = v0.uv0 * bary.x + v1.uv0 * bary.y + v2.uv0 * bary.z;
	surface.material_idx = pinfo.material_index;
	surface.area = 0.5 * length(world_cross);
	return surface;
}

#endif
