#include "../../commons.h"

struct PCIRCache {
	vec3 sky_col;
	uint frame_num;
	vec3 min_bounds;
	uint size_x;
	vec3 max_bounds;
	uint size_y;
	int num_lights;
	float total_light_area;
	int light_triangle_count;
	uint dir_light_idx;
	uint direct_lighting;
	float min_cell_size;
	float desired_px_per_cell;
	uint max_hash_table_size;
	int rand;
	vec3 pad;
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

NAMESPACE_END()