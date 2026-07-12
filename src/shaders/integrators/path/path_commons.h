#include "../../commons.h"

struct PCPath {
	vec3 sky_col;
	uint frame_num;
	uint width;
	uint height;
	int num_lights;
	uint time;
	int max_depth;
	uint dir_light_idx;
	uint direct_lighting;
	uint enable_accumulation;
};
