#ifndef IR_COMMONS_H
#define IR_COMMONS_H
#include "../../commons.h"

#define SURFELIZE_PASS_TILE_SIZE_XY 16

#define ALLOCATE_PASS_WG_SIZE 256

#define SUBGROUP_SIZE 32

#define SCAN_WG_SIZE 128

#define DEFAULT_WG_SIZE 128

// 256K surfels
#define MAX_SURFEL_COUNT (256 * 1024)

// Center cell count isn't quite arbitrary
// It should be below abs(1.0 / (2.0 * surfel_radius_factor))
// Reason: We want to fit a surfel inside a grid with 2 x radius
// Otherwise the surfel cell exceed the grid cell size
// Which breaks some assumptions
#define GRID_CENTER_CELL_COUNT_AXIS 64
#define GRID_TRAPEZOIDAL_CELL_COUNT_AXIS 64
#define GRID_AVG_SURFELS_PER_CELL 16

#define IRCACHE_DEBUG_VIEW_GBUFFER_ALBEDO 0
#define IRCACHE_DEBUG_VIEW_GBUFFER_NORMALS 1
#define IRCACHE_DEBUG_VIEW_TRAPEZOIDAL_GRID 2
#define IRCACHE_DEBUG_VIEW_TRAPEZOIDAL_GRID_SURFELS 3
#define IRCACHE_DEBUG_VIEW_COUNT 4

struct PCIRCache {
	vec3 sky_col;
	uint frame_num;
	vec3 min_bounds;
	uint width;
	vec3 max_bounds;
	uint height;
	int num_lights;
	float total_light_area;
	int total_light_count;
	uint dir_light_idx;
	uint direct_lighting;
	uint sampling_seed;
	float desired_surfel_radius_px;
	uint grid_total_cells;
	float grid_uniform_cell_distance_threshold;
	float scene_extent;
	uint total_frame_num;
	uint rays_per_surfel;
	uint use_camera_relative_surfel_size;
	uint debug_view;
};

struct PCPrefixSum {
	uint scan_sums;
	uint num_elems;
	uint block_sum_offset;
	uint out_offset;
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

struct Surfel {
	GBuffer gbuffer;
	float radius;
	uint age;
	vec3 irradiance;
	uint flags;
};

struct SurfelSample {
	vec3 radiance;
	float hit_t;
};

NAMESPACE_END()
#endif
