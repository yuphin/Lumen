#include "../../commons.h"
struct AtomicData {
	vec3 min_bnds;
	vec3 max_bnds;
	ivec3 grid_res;
	float max_radius;
};

struct PCSPPM {
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
	uint random_num;
	float radius;
	float ppm_base_radius;
};

struct SPPMData {
	vec3 p;
	vec3 wo;
	vec3 tau;
	vec3 col;
	vec3 phi;
	vec3 throughput;
	uint material_idx;
	vec2 uv;
	vec3 n_s;
	int M;
	float N;
	float radius;
	int path_len;
};

struct PhotonHash {
	vec3 pos;
	vec3 wi;
	vec3 throughput;
	int photon_count;
	vec3 nrm;
	uint path_len;
};