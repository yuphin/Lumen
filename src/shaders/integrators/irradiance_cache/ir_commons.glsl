#include "ir_commons.h"
#include "../../commons.glsl"
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

uint xxhash32(uint p) {
	const uint PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
	const uint PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
	uint h32 = p + PRIME32_5;
	h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
	h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
	h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
	return h32 ^ (h32 >> 16);
}

uint xxhash32(uvec2 p) {
	const uint PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
	const uint PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
	uint h32 = p.y + PRIME32_5 + p.x * PRIME32_3;
	h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
	h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
	h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
	return h32 ^ (h32 >> 16);
}

uint xxhash32(uvec3 p) {
	const uint PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
	const uint PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
	uint h32 = p.z + PRIME32_5 + p.x * PRIME32_3;
	h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
	h32 += p.y * PRIME32_3;
	h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
	h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
	h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
	return h32 ^ (h32 >> 16);
}

uint xxhash32(uvec4 p) {
	const uint PRIME32_2 = 2246822519U, PRIME32_3 = 3266489917U;
	const uint PRIME32_4 = 668265263U, PRIME32_5 = 374761393U;
	uint h32 = p.w + PRIME32_5 + p.x * PRIME32_3;
	h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
	h32 += p.y * PRIME32_3;
	h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
	h32 += p.z * PRIME32_3;
	h32 = PRIME32_4 * ((h32 << 17) | (h32 >> (32 - 17)));
	h32 = PRIME32_2 * (h32 ^ (h32 >> 15));
	h32 = PRIME32_3 * (h32 ^ (h32 >> 13));
	return h32 ^ (h32 >> 16);
}

vec3 debug_col(uint num) {
	const float INV_11 = 1.0 / 2047.0;
	const float INV_10 = 1.0 / 1023.0;
	return vec3((float(num & 0x7FFu)) * INV_11, (float((num >> 11) & 0x7FFu)) * INV_11,
				(float((num >> 22) & 0x3FFu)) * INV_10);
}

vec3 uvec3_to_random_color(uvec3 value) { return debug_col(xxhash32(value)); }
vec3 uint_to_random_color(uint value) { return debug_col((value)); }


float compute_cell_size(vec3 pos, vec3 cam_pos, float fovy, float viewport_height, float desired_px_size) {
	vec3 to_pos = pos - cam_pos;
	return (2 * desired_px_size * max(abs(to_pos.x), max(abs(to_pos.y), abs(to_pos.z))) * tan(radians(fovy / 2))) /
		   viewport_height;
}

uvec3 quantize_pos(vec3 pos, vec3 cam_pos, float fovy, float viewport_height, float desired_px_size,
				   float base_cell_size, vec3 min_bounds) {
	float s_k = compute_cell_size(pos, cam_pos, fovy, viewport_height, desired_px_size);
	float level = max(0.0, ceil(log2(s_k / base_cell_size)));
	float s_cell = base_cell_size * exp2(level);
	vec3 aligned_pos = pos - min_bounds;
	return uvec3(floor(aligned_pos / s_cell));
}

#if 1
vec2 octahedral_encode(vec3 n) {
	vec2 p = n.xy / (abs(n.x) + abs(n.y) + abs(n.z));
	if (n.z < 0.0) {
		p = (1.0 - abs(p.yx)) * sign(p);
	}
	return p * 0.5 + 0.5;
}

uint quantize_normal(vec3 normal) {
	const uint QUANT_BITS = 4;
	const uint MAX_QUANT = 1 << QUANT_BITS;

	vec2 oct = octahedral_encode(normal);

	uvec2 quantized = min(uvec2(oct * float(MAX_QUANT)), uvec2(MAX_QUANT - 1));

	return quantized.y * MAX_QUANT + quantized.x;
}
#else
uint quantize_normal(vec3 normal) {
	const uint MAX_QUANT = 1 << 4;
	float theta = acos(clamp(normal.z, -1.0, 1.0));
	float phi = atan(normal.y, normal.x);
	if (phi < 0.0) phi += 2.0 * PI;
	uint theta_packed = min(uint((theta / PI) * float(MAX_QUANT)), MAX_QUANT - 1);
	uint phi_packed = min(uint((phi / (2.0 * PI)) * float(MAX_QUANT)), MAX_QUANT - 1);

	return theta_packed * MAX_QUANT + phi_packed;
}
#endif

uint hash_key(HashKey key) { return xxhash32(uvec2(xxhash32(uvec4(key.pos, key.normal)), key.material)); }