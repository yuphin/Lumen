#include "../../../commons.h"
#include "restir_structs.h"
struct PCReSTIR {
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
	uint do_spatiotemporal;
	uint random_num;
	int enable_accumulation;
};
