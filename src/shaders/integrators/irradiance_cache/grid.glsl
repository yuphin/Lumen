#define GRID_TYPE_CELL 0
#define GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS 1
#define GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS 2
#define GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS 3
#define GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS 4
#define GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS 5
#define GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS 6

#define GRID_LOOKUP_TRAPEZOIDAL_NEIGHBORS 0
#define GRID_LOOKUP_BBOX 1

#define GRID_LOOKUP_MODE GRID_LOOKUP_TRAPEZOIDAL_NEIGHBORS
// #define GRID_LOOKUP_MODE GRID_LOOKUP_BBOX

uint floor_cell(float value, uint cell_count) { return uint(clamp(int(floor(value)), 0, int(cell_count) - 1)); }

uvec3 floor_cell(vec3 value, uint cell_count) {
	return uvec3(clamp(ivec3(floor(value)), ivec3(0), ivec3(int(cell_count) - 1)));
}

uint ceil_cell(float value, uint cell_count) { return uint(clamp(int(ceil(value)), 0, int(cell_count) - 1)); }

uvec3 ceil_cell(vec3 value, uint cell_count) {
	return uvec3(clamp(ivec3(ceil(value)), ivec3(0), ivec3(int(cell_count) - 1)));
}

vec3 center_grid_position(uvec3 ijk) {
	float cell_size = 2.0 * pc.grid_uniform_cell_distance_threshold / float(GRID_CENTER_CELL_COUNT_AXIS);
	return vec3(ijk) * cell_size - vec3(pc.grid_uniform_cell_distance_threshold);
}

vec3 trapezoidal_grid_position(uint region, uint i, uint j, uint k, float depth) {
	// The goal is to get the neighborhood per cell
	// For this we need to inspect the 4 corners of the cell
	// Eg. if it's +X , it's left corner can belong -Z . But also - Y
	// Detecting this is easy. We just need to check abs(x) == abs(z) or abs(y) == abs(z) etc etc.

	// The corner selection depends on the face:

	// +X: start (u,v) = BL
	// (u, u+ 1) x (v, v + 1) (_x_ indicates cartesian product)

	//  For example, the above will yield:
	// (u,v) , (u, v +1), (u + 1, v), (u + 1, v + 1)
	// that corresponds to BL, TL, BR, TR respectively

	// -X : start = BR
	// (u, u + 1) x (v, v +1)

	// -Z: start = BL
	// 	(u, u + 1) x (v, v + 1)

	// +Z: start = BR
	// (u, u + 1) x (v, v + 1)

	// +Y; start = BL
	// (u, u + 1) x (v, v + 1)

	// -Y: start = TL
	// (u, u + 1) x (v, v + 1)

	const float N = float(GRID_TRAPEZOIDAL_CELL_COUNT_AXIS);
	float u = (float(i) / N) * 2.0 - 1.0;
	float v = (float(j) / N) * 2.0 - 1.0;
	float max_ratio = 1e6 / pc.grid_uniform_cell_distance_threshold;
	// float depth = pc.grid_uniform_cell_distance_threshold * pow(max_ratio, float(k + 1) / N);

	if (region == GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS) {
		// +X, move to the left
		return vec3(depth, u * depth, v * depth);
	} else if (region == GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS) {
		return vec3(-depth, u * depth, v * depth);
	} else if (region == GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS) {
		return vec3(u * depth, depth, v * depth);
	} else if (region == GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS) {
		return vec3(u * depth, -depth, v * depth);
	} else if (region == GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS) {
		return vec3(u * depth, v * depth, depth);
	}
	return vec3(u * depth, v * depth, -depth);
}

uvec4 map_grid_axis(vec3 pos, out vec3 grid_pos_begin, out vec3 grid_pos_end) {
	vec3 pos_abs = abs(pos);
	float max_dir = max(pos_abs.x, pos_abs.y);
	max_dir = max(max_dir, pos_abs.z);

	if (max_dir <= pc.grid_uniform_cell_distance_threshold) {
		uvec3 ijk = floor_cell(
			((pos / pc.grid_uniform_cell_distance_threshold) + vec3(1.0)) * vec3(0.5) * GRID_CENTER_CELL_COUNT_AXIS,
			GRID_CENTER_CELL_COUNT_AXIS);
		uvec3 ijk_end = ceil_cell(
			((pos / pc.grid_uniform_cell_distance_threshold) + vec3(1.0)) * vec3(0.5) * GRID_CENTER_CELL_COUNT_AXIS,
			GRID_CENTER_CELL_COUNT_AXIS);

		grid_pos_begin = center_grid_position(ijk);
		grid_pos_end = center_grid_position(ijk_end);
		return uvec4(GRID_TYPE_CELL, ijk);
	}
	uint region;
	float depth;
	float u;
	float v;
	if (max_dir == pos_abs.x) {
		region = pos.x > 0 ? GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS : GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS;
		depth = pos_abs.x;
		u = pos.y / depth;
		v = pos.z / depth;
	} else if (max_dir == pos_abs.y) {
		region = pos.y > 0 ? GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS : GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS;
		depth = pos_abs.y;
		u = pos.x / depth;
		v = pos.z / depth;

	} else {
		region = pos.z > 0 ? GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS : GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS;
		depth = pos_abs.z;
		u = pos.x / depth;
		v = pos.y / depth;
	}

	uint i = floor_cell((u + 1.0) * 0.5 * GRID_TRAPEZOIDAL_CELL_COUNT_AXIS, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS);
	uint j = floor_cell((v + 1.0) * 0.5 * GRID_TRAPEZOIDAL_CELL_COUNT_AXIS, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS);

	float depth_ratio = depth / pc.grid_uniform_cell_distance_threshold;
	float max_ratio = 1e6 / pc.grid_uniform_cell_distance_threshold;
	float k_normalized = log(depth_ratio) / log(max_ratio);
	uint k = floor_cell(k_normalized * GRID_TRAPEZOIDAL_CELL_COUNT_AXIS, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS);

	grid_pos_begin = trapezoidal_grid_position(region, i, j, k, depth);
	grid_pos_end = trapezoidal_grid_position(region, i + 1, j + 1, k + 1, depth);
	return uvec4(region, i, j, k);
}

uvec4 map_grid_axis(vec3 pos) {
	vec3 unused_begin;
	vec3 unused_end;
	return map_grid_axis(pos, unused_begin, unused_end);
}

uint linearize_grid(uvec4 grid_pos) {
	if (grid_pos.x == GRID_TYPE_CELL) {
		uint N = GRID_CENTER_CELL_COUNT_AXIS;
		return grid_pos.y + grid_pos.z * N + grid_pos.w * N * N;
	} else {
		uint N = GRID_TRAPEZOIDAL_CELL_COUNT_AXIS;
		uint Nc = GRID_CENTER_CELL_COUNT_AXIS;
		return Nc * Nc * Nc + (grid_pos.x - 1) * (N * N * N) + grid_pos.y + grid_pos.z * N + grid_pos.w * N * N;
	}
}

// "depth" for the trapezoidal cell
float get_depth(uint region, vec3 pos) {
	switch (region) {
		case GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS:
		case GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS:
			return abs(pos.x);
		case GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS:
		case GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS:
			return abs(pos.y);
		case GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS:
		case GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS:
			return abs(pos.z);
		default:
			return 0.0;
	}
}

// Compute cell relative UV
// Bottom left = (0,0)
// Top right = (1,1)
vec2 compute_normalized_uvs(vec3 uv, uint region) {
	switch (region) {
		case GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS:
			return uv.zy;
		case GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS:
			return vec2(1.0 - uv.z, uv.y);
		case GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS:
			return uv.xz;
		case GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS:
			return vec2(uv.x, 1.0 - uv.z);
		case GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS:
			return vec2(1.0 - uv.x, uv.y);
		case GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS:
			return uv.xy;
		default:
			return vec2(0);
	};
}


////////////////////////////
// --- The following comment is a verbose description of the trapezoidal grid neighborhood rules ---
// The code for the neighborhood rules is implemented based on the description below.
/*
Let (x,y,z) cell ID
Neighborhood is according to NDC bottom left location for cell
PLUS_X:
Edge cases:
cell_begin for neighboring MINUS_Z (x == -z) and MINUS_Y (x == -y)
cell_end for neighboring PLUS_Y (x == y) and PLUS_Z (x == z)

left neighbor (MINUS_Z) = (GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, x, z)
bottom neighbor (MINUS_Y) = (GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, y, z)
top neighbor = (PLUS_Y) = (GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, y, z)
right_neighbor (PLUS_Z) = (GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, x, z)

top_left (PLUS_Y) = (GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, y - 1, z)
 --- OR (MINUS_Z) =  (GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, x + 1, z) (the horizontal case is omitted from now) ---
top_right (PLUS_Y) = (GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, y + 1, z)
bottom left (MINUS_Y) = (GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, y - 1, z)
bottom right (MINUS_Y) = (GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, y + 1, z)

MINUS_Z
Edge cases:
cell_begin for neighboring MINUS_X (x == z) and MINUS_Y (y == z)
cell_end for neighboring PLUS_Y (y == -z) and PLUS_X (x == -z)

left neighbor (MINUS_X) = (y, 0, z)
bottom neighbor (MINUS_Y) = (x,0,z)
top neighbor = (PLUS_Y) = (x, 0, z)
right_neighbor (PLUS_X) = (y, 0, z)

top left (PLUS_Y) = (x - 1, 0, z)
top right (PLUS_Y) = (x + 1, 0, z)
bottom left (MINUS_Y) = (x - 1, 0, z)
bottom right (MINUS_Y) = (x + 1, 0, z)

MINUS_Y
Edge cases:
cell_begin for neighboring MINUS_X (x == y) and MINUS_Z (y == z)
cell_end for neighboring PLUS_Z (y == -z) and PLUS_X (x == -y)

left neighbor (MINUS_X) = (0,y,z)
bottom neighbor (PLUS_Z) = (x, 0, z)
top neighbor = (MINUS_Z) = (x,0,z)
right_neighbor (PLUS_X) = (0, y, z)

top left (MINUS_Z) = (x - 1, 0, z)
top right (MINUS_Z) = (x + 1, 0, z)
bottom left (PLUS_Z) = (x - 1, 0, z)
bottom right (PLUS_Z) = (x + 1, 0, z)

MINUS_X
Edge cases:
cell_begin for neighboring MINUS_Z (x == z) and MINUS_Y (x == y)
cell_end for neighboring PLUS_Y (x == -y) and PLUS_Z (x == -z)
left neighbor (PLUS_Z) = (0, x, z)
bottom neighbor (MINUS_Y) = (0, y, z)
top neighbor = (PLUS_Y) = (0, y, z)
right_neighbor (MINUS_Z) = (0, x, z)

top left (PLUS_Y) = (0, y + 1, z)
top right (PLUS_Y) = (0, y - 1, z)
bottom left (MINUS_Y) = (0, y + 1, z)
bottom right (MINUS_Y) = (0, y - 1, z)

PLUS_Z
Edge cases:
cell_begin for neighboring MINUS_X (x == -z) and MINUS_Y (y == -z)
cell_end for neighboring PLUS_Y (y == z) and PLUS_X (x == z)

left neighbor (PLUS_X) = (y, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, z)
bottom neighbor (MINUS_Y) = (x, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1,z)
top neighbor = (PLUS_Y) = (x, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS -1, z)
right_neighbor (MINUS_X) = (y, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS -1, z)

top left (PLUS_Y) = (x + 1, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, z)
top right (PLUS_Y) =  (x - 1, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, z)
bottom left (MINUS_Y) = (x + 1, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, z)
bottom right (MINUS_Y) = (x - 1, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, z)

PLUS_Y
Edge cases:
cell_begin for neighboring MINUS_X (x == -y) and MINUS_Z (y == -z)
cell_end for neighboring PLUS_Z (y == z) and PLUS_X (x == y)

left neighbor (MINUS_X) = (GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1,y,z)
bottom neighbor (MINUS_Z) = (x, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, z)
top neighbor = (PLUS_Z) = (x, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1,z)
right_neighbor (PLUS_X) = (GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, y, z)

top left(PLUS_Z) = (x - 1, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, z)
top right(PLUS_Z) = (x + 1, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, z)
bottom left (MINUS_Z) = (x- 1, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, z)
bottom right (MINUS_Z) = (x + 1, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1, z)
*/

uvec4 left_neighbor(uvec3 cell_id, uint region) {
	uint N = GRID_TRAPEZOIDAL_CELL_COUNT_AXIS;
	uint i = cell_id.x, j = cell_id.y, k = cell_id.z;
	switch (region) {
		case GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS:
			return j > 0 ? uvec4(region, i, j - 1, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS, N - 1, i, k);
		case GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS:
			return j < N - 1 ? uvec4(region, i, j + 1, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS, 0, i, k);
		case GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS:
			return i > 0 ? uvec4(region, i - 1, j, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS, N - 1, j, k);
		case GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS:
			return i > 0 ? uvec4(region, i - 1, j, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS, 0, j, k);
		case GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS:
			return i < N - 1 ? uvec4(region, i + 1, j, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS, j, N - 1, k);
		case GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS:
			return i > 0 ? uvec4(region, i - 1, j, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS, j, 0, k);
		default:
			return uvec4(region, cell_id);
	}
}

uvec4 right_neighbor(uvec3 cell_id, uint region) {
	uint N = GRID_TRAPEZOIDAL_CELL_COUNT_AXIS;
	uint i = cell_id.x, j = cell_id.y, k = cell_id.z;
	switch (region) {
		case GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS:
			return j < N - 1 ? uvec4(region, i, j + 1, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS, N - 1, i, k);
		case GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS:
			return j > 0 ? uvec4(region, i, j - 1, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS, 0, i, k);
		case GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS:
			return i < N - 1 ? uvec4(region, i + 1, j, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS, N - 1, j, k);
		case GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS:
			return i < N - 1 ? uvec4(region, i + 1, j, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS, 0, j, k);
		case GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS:
			return i > 0 ? uvec4(region, i - 1, j, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS, j, N - 1, k);
		case GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS:
			return i < N - 1 ? uvec4(region, i + 1, j, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS, j, 0, k);
		default:
			return uvec4(region, cell_id);
	}
}

uvec4 bottom_neighbor(uvec3 cell_id, uint region) {
	uint N = GRID_TRAPEZOIDAL_CELL_COUNT_AXIS;
	uint i = cell_id.x, j = cell_id.y, k = cell_id.z;
	switch (region) {
		case GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS:
			return i > 0 ? uvec4(region, i - 1, j, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS, N - 1, j, k);
		case GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS:
			return i > 0 ? uvec4(region, i - 1, j, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS, 0, j, k);
		case GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS:
			return j > 0 ? uvec4(region, i, j - 1, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS, i, N - 1, k);
		case GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS:
			return j < N - 1 ? uvec4(region, i, j + 1, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS, i, 0, k);
		case GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS:
			return j > 0 ? uvec4(region, i, j - 1, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS, i, N - 1, k);
		case GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS:
			return j > 0 ? uvec4(region, i, j - 1, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS, i, 0, k);
		default:
			return uvec4(region, cell_id);
	}
}

uvec4 top_neighbor(uvec3 cell_id, uint region) {
	uint N = GRID_TRAPEZOIDAL_CELL_COUNT_AXIS;
	uint i = cell_id.x, j = cell_id.y, k = cell_id.z;
	switch (region) {
		case GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS:
			return i < N - 1 ? uvec4(region, i + 1, j, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS, N - 1, j, k);
		case GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS:
			return i < N - 1 ? uvec4(region, i + 1, j, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS, 0, j, k);
		case GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS:
			return j < N - 1 ? uvec4(region, i, j + 1, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS, i, N - 1, k);
		case GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS:
			return j > 0 ? uvec4(region, i, j - 1, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS, i, 0, k);
		case GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS:
			return j < N - 1 ? uvec4(region, i, j + 1, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS, i, N - 1, k);
		case GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS:
			return j < N - 1 ? uvec4(region, i, j + 1, k) : uvec4(GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS, i, 0, k);
		default:
			return uvec4(region, cell_id);
	}
}

uvec4 top_left_neighbor(uvec3 cell_id, uint region) {
	uvec4 h = left_neighbor(cell_id, region);
	if (h.x == region) return top_neighbor(h.yzw, h.x);
	uvec4 v = top_neighbor(cell_id, region);
	if (v.x == region) return left_neighbor(v.yzw, v.x);
	return v;
}

uvec4 top_right_neighbor(uvec3 cell_id, uint region) {
	uvec4 h = right_neighbor(cell_id, region);
	if (h.x == region) return top_neighbor(h.yzw, h.x);
	uvec4 v = top_neighbor(cell_id, region);
	if (v.x == region) return right_neighbor(v.yzw, v.x);
	return v;
}

uvec4 bottom_left_neighbor(uvec3 cell_id, uint region) {
	uvec4 h = left_neighbor(cell_id, region);
	if (h.x == region) return bottom_neighbor(h.yzw, h.x);
	uvec4 v = bottom_neighbor(cell_id, region);
	if (v.x == region) return left_neighbor(v.yzw, v.x);
	return v;
}

uvec4 bottom_right_neighbor(uvec3 cell_id, uint region) {
	uvec4 h = right_neighbor(cell_id, region);
	if (h.x == region) return bottom_neighbor(h.yzw, h.x);
	uvec4 v = bottom_neighbor(cell_id, region);
	if (v.x == region) return right_neighbor(v.yzw, v.x);
	return v;
}

#define INCLUDE_DEPTH

#ifdef INCLUDE_DEPTH
#define MAX_NEIGHBOR_PLUS_ITSELF_COUNT 8
#else
#define MAX_NEIGHBOR_PLUS_ITSELF_COUNT 4
#endif	// INCLUDE_DEPTH
void collect_neighbors(vec3 surfel_pos, float surfel_radius, vec3 cam_pos,
					   inout uvec4 neighbors[MAX_NEIGHBOR_PLUS_ITSELF_COUNT], out uint neighbor_count) {
	// Determine the neighbor cells that the surfel is touching
	vec3 surfel_grid_pos_begin;
	vec3 surfel_grid_pos_end;
	vec3 surfel_cam_relative_pos = surfel_pos - cam_pos;
	uvec4 surfel_grid_val = map_grid_axis(surfel_cam_relative_pos, surfel_grid_pos_begin, surfel_grid_pos_end);

	if (surfel_grid_val.x == GRID_TYPE_CELL) {
		neighbors[0] = surfel_grid_val;
		neighbor_count = 1;
		return;
	}

	vec3 surfel_uv = (surfel_cam_relative_pos - surfel_grid_pos_begin) / (surfel_grid_pos_end - surfel_grid_pos_begin);
	vec2 normalized_uv_surfel = compute_normalized_uvs(surfel_uv, surfel_grid_val.x);

	vec3 surfel_grid_bbox = surfel_grid_pos_end - surfel_grid_pos_begin;
	float surfel_cell_edge = max(max(abs(surfel_grid_bbox.x), abs(surfel_grid_bbox.y)), abs(surfel_grid_bbox.z));
	float radius_in_uv = surfel_radius / surfel_cell_edge;

	const float CELL_EPS = 1e-3;
	bool neighboring_left = (normalized_uv_surfel.x - radius_in_uv) < -CELL_EPS;
	bool neighboring_right = (normalized_uv_surfel.x + radius_in_uv) > (1.0 + CELL_EPS);
	bool neighboring_bottom = (normalized_uv_surfel.y - radius_in_uv) < -CELL_EPS;
	bool neighboring_top = (normalized_uv_surfel.y + radius_in_uv) > (1.0 + CELL_EPS);

	// Diagonals
	float r2 = radius_in_uv * radius_in_uv;
	vec2 d_tl = vec2(0.0, 1.0) - normalized_uv_surfel;
	bool neighboring_top_left = dot(d_tl, d_tl) < r2;
	vec2 d_tr = vec2(1.0, 1.0) - normalized_uv_surfel;
	bool neighboring_top_right = dot(d_tr, d_tr) < r2;
	vec2 d_bl = vec2(0.0, 0.0) - normalized_uv_surfel;
	bool neighboring_bottom_left = dot(d_bl, d_bl) < r2;
	vec2 d_br = vec2(1.0, 0.0) - normalized_uv_surfel;
	bool neighboring_bottom_right = dot(d_br, d_br) < r2;

#ifdef INCLUDE_DEPTH
	float max_ratio = 1e6 / pc.grid_uniform_cell_distance_threshold;
	float Nf = float(GRID_TRAPEZOIDAL_CELL_COUNT_AXIS);
	float depth_begin = pc.grid_uniform_cell_distance_threshold * pow(max_ratio, float(surfel_grid_val.w) / Nf);
	float depth_end = pc.grid_uniform_cell_distance_threshold * pow(max_ratio, float(surfel_grid_val.w + 1) / Nf);
	float depth_curr = get_depth(surfel_grid_val.x, surfel_cam_relative_pos);

	bool crossing_down = (depth_curr - surfel_radius < depth_begin) && surfel_grid_val.w > 0;
	bool crossing_up =
		(depth_curr + surfel_radius > depth_end) && surfel_grid_val.w < GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1;
	bool depth_diff = crossing_down || crossing_up;
	int direction = crossing_down ? -1 : 1;
#endif

	neighbor_count = 0;
	// Itself
	neighbors[neighbor_count++] = surfel_grid_val;

#ifdef INCLUDE_DEPTH
	if (depth_diff) {
		neighbors[neighbor_count] =
			uvec4(neighbors[neighbor_count - 1].xyz, neighbors[neighbor_count - 1].w + uint(direction));
		neighbor_count++;
	}
#endif	// INCLUDE_DEPTH

	// Can at most touch 3 other neighbors
	if (neighboring_left) {
		neighbors[neighbor_count++] = left_neighbor(surfel_grid_val.yzw, surfel_grid_val.x);
#ifdef INCLUDE_DEPTH
		if (depth_diff) {
			neighbors[neighbor_count] =
				uvec4(neighbors[neighbor_count - 1].xyz, neighbors[neighbor_count - 1].w + uint(direction));
			neighbor_count++;
		}
#endif	// INCLUDE_DEPTH
	}
	if (neighboring_right) {
		neighbors[neighbor_count++] = right_neighbor(surfel_grid_val.yzw, surfel_grid_val.x);
#ifdef INCLUDE_DEPTH
		if (depth_diff) {
			neighbors[neighbor_count] =
				uvec4(neighbors[neighbor_count - 1].xyz, neighbors[neighbor_count - 1].w + uint(direction));
			neighbor_count++;
		}
#endif	// INCLUDE_DEPTH
	}
	if (neighboring_bottom) {
		neighbors[neighbor_count++] = bottom_neighbor(surfel_grid_val.yzw, surfel_grid_val.x);
#ifdef INCLUDE_DEPTH
		if (depth_diff) {
			neighbors[neighbor_count] =
				uvec4(neighbors[neighbor_count - 1].xyz, neighbors[neighbor_count - 1].w + uint(direction));
			neighbor_count++;
		}
#endif	// INCLUDE_DEPTH
	}
	if (neighboring_top) {
		neighbors[neighbor_count++] = top_neighbor(surfel_grid_val.yzw, surfel_grid_val.x);
#ifdef INCLUDE_DEPTH
		if (depth_diff) {
			neighbors[neighbor_count] =
				uvec4(neighbors[neighbor_count - 1].xyz, neighbors[neighbor_count - 1].w + uint(direction));
			neighbor_count++;
		}
#endif	// INCLUDE_DEPTH
	}
	if (neighboring_top_left) {
		neighbors[neighbor_count++] = top_left_neighbor(surfel_grid_val.yzw, surfel_grid_val.x);
#ifdef INCLUDE_DEPTH
		if (depth_diff) {
			neighbors[neighbor_count] =
				uvec4(neighbors[neighbor_count - 1].xyz, neighbors[neighbor_count - 1].w + uint(direction));
			neighbor_count++;
		}
#endif	// INCLUDE_DEPTH
	}
	if (neighboring_top_right) {
		neighbors[neighbor_count++] = top_right_neighbor(surfel_grid_val.yzw, surfel_grid_val.x);
#ifdef INCLUDE_DEPTH
		if (depth_diff) {
			neighbors[neighbor_count] =
				uvec4(neighbors[neighbor_count - 1].xyz, neighbors[neighbor_count - 1].w + uint(direction));
			neighbor_count++;
		}
#endif	// INCLUDE_DEPTH
	}
	if (neighboring_bottom_left) {
		neighbors[neighbor_count++] = bottom_left_neighbor(surfel_grid_val.yzw, surfel_grid_val.x);
#ifdef INCLUDE_DEPTH
		if (depth_diff) {
			neighbors[neighbor_count] =
				uvec4(neighbors[neighbor_count - 1].xyz, neighbors[neighbor_count - 1].w + uint(direction));
			neighbor_count++;
		}
#endif	// INCLUDE_DEPTH
	}
	if (neighboring_bottom_right) {
		neighbors[neighbor_count++] = bottom_right_neighbor(surfel_grid_val.yzw, surfel_grid_val.x);
#ifdef INCLUDE_DEPTH
		if (depth_diff) {
			neighbors[neighbor_count] =
				uvec4(neighbors[neighbor_count - 1].xyz, neighbors[neighbor_count - 1].w + uint(direction));
			neighbor_count++;
		}
#endif	// INCLUDE_DEPTH
	}

	// Useful assertions
	if (neighbor_count > MAX_NEIGHBOR_PLUS_ITSELF_COUNT) {
		debugPrintfEXT("Max neighbor count exceeded %d!\n", neighbor_count);
	}

	if (radius_in_uv > 0.5) {
		debugPrintfEXT("Surfel radius exceeds cell size! radius_in_uv: %f, surfel_radius: %f, surfel_cell_edge: %f\n",
					   radius_in_uv, surfel_radius, surfel_cell_edge);
	}

#ifdef PRINT_GRID

	debugPrintfEXT("%v3f - %v3f\n", surfel_grid_pos_begin, surfel_grid_pos_end);
	debugPrintfEXT("grid %v3u - ", surfel_grid_val.yzw);
	debugPrintfEXT(
		"left %d - right %d - bottom %d - top %d - top_left %d - top_right %d - bottom_left %d - bottom_right %d\n",
		neighboring_left, neighboring_right, neighboring_bottom, neighboring_top, neighboring_top_left,
		neighboring_top_right, neighboring_bottom_left, neighboring_bottom_right);
	if (neighbor_count > 0) {
		for (uint i = 0; i < neighbor_count; i++) {
			uvec4 n = neighbors[i];
			debugPrintfEXT("neighbor %d - region %d - grid: %v3u\n", i, n.x, n.yzw);
		}
	}
	debugPrintfEXT("----\n");
#endif
}