#ifndef RANDOM_GLSL
#define RANDOM_GLSL
// Returns a float between 0 and 1
float uint_to_float(uint x) { return uintBitsToFloat(0x3f800000 | (x >> 9)) - 1.0f; }

// PCG random numbers generator
// Source: "Hash Functions for GPU Rendering" by Jarzynski & Olano
uvec4 pcg4d(uvec4 v) {
	v = v * 1664525u + 1013904223u;
	v.x += v.y * v.w;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v.w += v.y * v.z;
	v = v ^ (v >> 16u);
	v.x += v.y * v.w;
	v.y += v.z * v.x;
	v.z += v.x * v.y;
	v.w += v.y * v.z;
	return v;
}

uvec4 init_rng(uvec2 pixel_coords, uvec2 resolution, uint frame_num, uint state) {
	return uvec4(pixel_coords.xy, frame_num, state);
}

uvec4 init_rng(uvec2 pixel_coords, uvec2 resolution, uint frame_num) { return uvec4(pixel_coords.xy, frame_num, 0); }

// Return random float in (0, 1) range
float rand(inout uvec4 rng_state) {
	rng_state.w++;
	return uint_to_float(pcg4d(rng_state).x);
}

vec2 rand2(inout uvec4 rng_state) { return vec2(rand(rng_state), rand(rng_state)); }

vec3 rand3(inout uvec4 rng_state) { return vec3(rand2(rng_state), rand(rng_state)); }

vec4 rand4(inout uvec4 rng_state) { return vec4(rand3(rng_state), rand(rng_state)); }
#endif