#include "../../commons.h"

struct PCPath {
	vec3 sky_col;
	uint frame_num;
	uint width;
	uint height;
	int num_lights;
	uint time;
	int max_depth;
	float total_light_area;
	int total_light_count;
	uint dir_light_idx;
	uint direct_lighting;
};