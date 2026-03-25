#ifndef IR_COMMONS_H
#define IR_COMMONS_H
#include "../../commons.h"

#define SURFELIZE_PASS_TILE_SIZE_XY 16

#define ALLOCATE_PASS_WG_SIZE 256

#define SUBGROUP_SIZE 32

#define SCAN_WG_SIZE 128

// 256K surfels
#define MAX_SURFEL_COUNT 256 * 1024 

// Center cell count isn't quite arbitrary
// It should be below abs(1.0 / (2.0 * surfel_radius_factor))
// Reason: We want to fit a surfel inside a grid with 2 x radius
// Otherwise the surfel cell exceed the grid cell size
// Which breaks some assumptions
#define GRID_CENTER_CELL_COUNT_AXIS 64
#define GRID_TRAPEZOIDAL_CELL_COUNT_AXIS 64
#define GRID_AVG_SURFELS_PER_CELL 16

struct PCIRCache {
	vec3 sky_col;
	uint frame_num;
	vec3 min_bounds;
	uint width;
	vec3 max_bounds;
	uint height;
	int num_lights;
	float total_light_area;
	int light_triangle_count;
	uint dir_light_idx;
	uint direct_lighting;
	int rand;
	float desired_surfel_radius_px;
	uint grid_total_cells;
	float grid_uniform_cell_distance_threshold;
	float scene_extent;
};

struct PCPrefixSum {
	uint scan_sums;
	uint num_elems;
	uint64_t block_sum_addr;
	uint64_t out_addr;
};

struct IRCacheUniforms {
	vec4 pad;
};

NAMESPACE_BEGIN(IRCache)

struct GBuffer {
	vec2 barycentrics;
	uvec2 primitive_instance_id;
};

struct IRCacheHitPayload {
	vec2 attribs;
	uint instance_idx;
	uint triangle_idx;
	float dist;
};

struct HashEntry {
	GBuffer representative_point;
	GBuffer hit_point;
	ivec2 pixel;
	// TODO: These can be optimized
	vec3 Li; // incoming radiance from the representative point
	float partial_jacobian;
};

struct Surfel {
	vec3 pos;
	float radius;
	vec3 n_s;
	uint age;
	vec3 irradiance;
	uint flags;
};

NAMESPACE_END()
#endif