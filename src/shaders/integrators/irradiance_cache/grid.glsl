#define GRID_TYPE_CELL 0
#define GRID_TYPE_TRAPEZOIDAL_PLUS_X_AXIS 1
#define GRID_TYPE_TRAPEZOIDAL_MINUS_X_AXIS 2
#define GRID_TYPE_TRAPEZOIDAL_PLUS_Y_AXIS 3
#define GRID_TYPE_TRAPEZOIDAL_MINUS_Y_AXIS 4
#define GRID_TYPE_TRAPEZOIDAL_PLUS_Z_AXIS 5
#define GRID_TYPE_TRAPEZOIDAL_MINUS_Z_AXIS 6
uvec4 map_grid_axis(vec3 pos) {
	vec3 pos_abs = abs(pos);
	float max_dir = max(pos_abs.x, pos_abs.y);
	max_dir = max(max_dir, pos_abs.z);

	if (max_dir <= pc.grid_uniform_cell_distance_threshold) {
		uvec3 ijk = uvec3(floor(((pos / pc.grid_uniform_cell_distance_threshold) + vec3(1.0)) * vec3(0.5) *
								GRID_CENTER_CELL_COUNT_AXIS));

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

	uint i = clamp(uint(floor((u + 1.0) * 0.5 * GRID_TRAPEZOIDAL_CELL_COUNT_AXIS)), 0,
				   GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1u);
	uint j = clamp(uint(floor((v + 1.0) * 0.5 * GRID_TRAPEZOIDAL_CELL_COUNT_AXIS)), 0,
				   GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1u);

#if 1
	float depth_ratio = depth / pc.grid_uniform_cell_distance_threshold;
	float max_ratio = 1e6 / pc.grid_uniform_cell_distance_threshold;
	float k_normalized = log(depth_ratio) / log(max_ratio);
	uint k =
		clamp(uint(floor(k_normalized * GRID_TRAPEZOIDAL_CELL_COUNT_AXIS)), 0, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1u);
#else
	float growth_ratio = 1.0 + (2.0 / float(GRID_TRAPEZOIDAL_CELL_COUNT_AXIS));
	float continuous_k = log(depth / pc.grid_uniform_cell_distance_threshold) / log(growth_ratio);
	uint k = clamp(uint(floor(continuous_k)), 0, GRID_TRAPEZOIDAL_CELL_COUNT_AXIS - 1u);
#endif

	return uvec4(region, i, j, k);
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