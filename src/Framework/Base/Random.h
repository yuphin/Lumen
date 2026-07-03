#pragma once
#include "Utils.h"

namespace lm {

struct PCG32 {
	u64 state;
	u64 inc;
};

inline void pcg32_seed(PCG32* rng, u64 seed, u64 stream = 1) {
	rng->state = 0;
	rng->inc = (stream << 1) | 1;
	rng->state = rng->state * 6364136223846793005ull + rng->inc;
	rng->state += seed;
	rng->state = rng->state * 6364136223846793005ull + rng->inc;
}

inline u32 pcg32_next(PCG32* rng) {
	u64 old_state = rng->state;
	rng->state = old_state * 6364136223846793005ull + rng->inc;
	u32 xorshifted = (u32)(((old_state >> 18) ^ old_state) >> 27);
	u32 rot = (u32)(old_state >> 59);
	return (xorshifted >> rot) | (xorshifted << ((32 - rot) & 31));
}

inline f32 pcg32_next_f32(PCG32* rng) {
	return (f32)(pcg32_next(rng) >> 8) * (1.0f / 16777216.0f);
}

//Note: Not thread safe.
inline PCG32 _global_rng = {0x853c49e6748fea9bull, 0xda3e39cb94b95bdbull};

inline void rand_seed(u64 seed) { pcg32_seed(&_global_rng, seed); }
inline u32 rand_u32() { return pcg32_next(&_global_rng); }

}  // namespace lm