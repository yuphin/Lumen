#define GRID_TYPE_CELL 0
#define GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS 1
#define GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS 2
#define GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS 3
#define GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS 4
#define GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS 5
#define GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS 6

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
