#include "../../bda.glsl"
SCENE_BUFFER_RO(transformations, mat4);
struct HitData {
	vec3 pos;
	vec3 n_g;
	vec3 n_s;
	vec2 uv;
	uint material_idx;
};

HitData get_hitdata(vec2 attribs, uint instance_idx, uint triangle_idx) {
	const PrimInfo pinfo = DEREF(prim_info)[instance_idx];
	const uint index_offset = pinfo.index_offset + 3 * triangle_idx;
	const ivec3 ind = ivec3(pinfo.vertex_offset) + ivec3(DEREF(index)[index_offset + 0], DEREF(index)[index_offset + 1],
														 DEREF(index)[index_offset + 2]);
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
	gbuffer.n_g = normalize((tsp_inv_to_world * vec4(cross(e0, e1), 0)).xyz);
	gbuffer.uv = vtx[0].uv0 * bary.x + vtx[1].uv0 * bary.y + vtx[2].uv0 * bary.z;
	gbuffer.material_idx = pinfo.material_index;
	return gbuffer;
}

HitData get_hitdata(GBuffer gbuffer) {
	return get_hitdata(gbuffer.barycentrics, gbuffer.primitive_instance_id.y, gbuffer.primitive_instance_id.x);
}

bool gbuffer_valid(GBuffer gbuffer) { return gbuffer.primitive_instance_id.x != uint(-1); }
