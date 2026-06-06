layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Transformation { mat4 m[]; };
Transformation transforms = Transformation(scene_desc.transformations_addr);
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

struct HashKey {
	uvec3 pos;
	uint normal;
	uint material;
};

HitData get_hitdata(vec2 attribs, uint instance_idx, uint triangle_idx, out float area) {
	const PrimInfo pinfo = prim_infos.d[instance_idx];
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

HitData get_hitdata(GBuffer gbuffer) {
	return get_hitdata(gbuffer.barycentrics, gbuffer.primitive_instance_id.y, gbuffer.primitive_instance_id.x);
}

HitDataWithoutUVAndGeometryNormals get_hitdata_no_ng_uv(vec2 attribs, uint instance_idx, uint triangle_idx) {
	const PrimInfo pinfo = prim_infos.d[instance_idx];
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
	const PrimInfo pinfo = prim_infos.d[instance_idx];
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
	const PrimInfo pinfo = prim_infos.d[instance_idx];
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

void init_gbuffer(out GBuffer gbuffer) {
	gbuffer.barycentrics = vec2(0);
	gbuffer.primitive_instance_id = uvec2(-1);
}

bool gbuffer_valid(GBuffer gbuffer) { return gbuffer.primitive_instance_id.x != uint(-1); }
