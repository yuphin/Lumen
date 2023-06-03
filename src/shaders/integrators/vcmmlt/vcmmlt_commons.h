#include "../mlt_commons.h"
#include "../vcm/vcm_commons.h"

struct VCMMLTSampler {
	uint last_large_step;
	uint iter;
	uint num_light_samples;
	float luminance;
	uint splat_cnt;
	uint past_splat_cnt;
	uint swap;
};

struct VCMMLTSeedData {
	uvec4 chain0_seed;
	uvec4 chain1_seed;
};

struct SumData {
	float x;  // lum accum
	uint y;	  // lum sample
	float z;
};