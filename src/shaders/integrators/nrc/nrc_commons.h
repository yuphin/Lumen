#include "../../commons.h"

#include "../restir/di/restir_structs.h"
struct PCNRC {
	vec3 sky_col;
	uint frame_num;
	vec3 min_bounds;
	uint size_x;
	vec3 max_bounds;
	uint size_y;
	int num_lights;
	uint time;
	int max_depth;
	float total_light_area;
	int light_triangle_count;
	uint dir_light_idx;
	uint tile_offset;
	uint total_frame_num;
	uint do_spatiotemporal;
	uint random_num;
	uint curr_mode;

};

struct RadianceQuery {
	vec3 position;
	float normal_phi;
	float normal_theta;
	float dir_phi;
	float dir_theta;
	float roughness;
	vec3 diffuse_reflectance;
	vec3 specular_reflectance;
};