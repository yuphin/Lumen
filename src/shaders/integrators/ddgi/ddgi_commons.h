#include "../../commons.h"

struct PCDDGI {
	mat4 probe_rotation;
	vec3 sky_col;
	uint frame_num;
	uint size_x;
	uint size_y;
	int num_lights;
	uint time;
	int max_depth;
	float total_light_area;
	int light_triangle_count;
	uint dir_light_idx;
	int first_frame;
	int infinite_bounces;
	int direct_lighting;
};

struct DDGIUniforms {
	ivec3 probe_counts;
	float hysteresis;
	vec3 probe_start_position;
	float probe_step;
	int rays_per_probe;
	float max_distance;
	float depth_sharpness;
	float normal_bias;
	float backface_ratio;
	int irradiance_width;
	int irradiance_height;
	int depth_width;
	int depth_height;
	float min_frontface_dist;
	float tmin;
	float tmax;
	vec3 pad;
};

struct GBufferData {
	vec3 pos;
	uint mat_idx;
	vec3 normal;
	uint pad;
	vec2 uv;
	vec2 pad2;
	vec3 albedo;
	uint pad3;
};

struct ALIGN16 SphereDesc {
	uint64_t index_addr;
	uint64_t vertex_addr;
};

struct SphereVertex {
	vec3 pos;
	vec3 normal;
};