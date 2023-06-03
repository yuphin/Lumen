#include "../commons.h"
#ifndef MLT_COMMONS_H
#define MLT_COMMONS_H

struct PCMLT {
	vec3 sky_col;
	uint frame_num;
	vec3 min_bounds;
	uint size_x;
	vec3 max_bounds;
	uint size_y;
	ivec3 grid_res;
	int num_lights;
	uint time;
	int max_depth;
	float total_light_area;
	int light_triangle_count;
	uint dir_light_idx;
	float mutations_per_pixel;
	uint light_rand_count;
	uint cam_rand_count;
	uint connection_rand_count;
	uint random_num;
	uint num_bootstrap_samples;
	uint mutation_counter;
	int use_vm;
	int use_vc;
	float radius;
	uint num_mlt_threads;
};


struct BootstrapSample {
	uvec4 seed;
	float lum;
};

struct SeedData {
	uvec4 chain_seed;
	int depth;
};

struct PrimarySample {
	float val;
	float backup;
	uint last_modified;
	uint last_modified_backup;
};

struct MLTSampler {
	uint last_large_step;
	uint iter;
	// uint sampler_idx;
	uint num_light_samples;
	uint num_cam_samples;
	uint num_connection_samples;
	float luminance;
	uint splat_cnt;
	uint past_splat_cnt;
	uint swap;
	uint type;
};

struct ChainData {
	float total_luminance;
	uint lum_samples;
	float total_samples;
	float normalization;
};

struct Splat {
	uint idx;
	vec3 L;
};
#endif