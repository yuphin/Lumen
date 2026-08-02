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
	uint width;
	vec3 max_bounds;
	uint height;
	ivec3 grid_res;
	int num_lights;
	uint time;
	int max_depth;
	uint dir_light_idx;
	uint random_num;
	float ppm_base_radius;
	uint enable_accumulation;
};

struct SPPMData {
	vec3 p;
	SurfaceRef surface;
	vec3 wo;
	vec3 tau;
	vec3 col;
	vec3 phi;
	vec3 throughput;
	int M;
	float N;
	float radius;
	int path_len;
};

#ifdef __cplusplus
static_assert(sizeof(SPPMData) == 104);
#endif

struct PhotonHash {
	vec3 pos;
	vec3 wi;
	vec3 throughput;
	int photon_count;
	vec3 nrm;
	uint path_len;
};
