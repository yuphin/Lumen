#ifndef ATOMIC_RGB_GLSL
#define ATOMIC_RGB_GLSL

// Assumes pixel major RGB storage -> [R0, G0, B0, R1, G1, B1, ...].
#define RGB_BUFFER_BASE(pixel_idx) (3u * uint(pixel_idx))

#define RGB_BUFFER_LOAD(field, pixel_idx)                                                                     \
	vec3(DEREF(field)[RGB_BUFFER_BASE(pixel_idx) + 0u], DEREF(field)[RGB_BUFFER_BASE(pixel_idx) + 1u],        \
		 DEREF(field)[RGB_BUFFER_BASE(pixel_idx) + 2u])

#define RGB_BUFFER_STORE(field, pixel_idx, value)                                                             \
	{                                                                                                         \
		const uint _rgb_buffer_base = RGB_BUFFER_BASE(pixel_idx);                                            \
		const vec3 _rgb_buffer_value = (value);                                                               \
		DEREF(field)[_rgb_buffer_base + 0u] = _rgb_buffer_value.x;                                           \
		DEREF(field)[_rgb_buffer_base + 1u] = _rgb_buffer_value.y;                                           \
		DEREF(field)[_rgb_buffer_base + 2u] = _rgb_buffer_value.z;                                           \
	}

#define RGB_BUFFER_ATOMIC_ADD(field, pixel_idx, value)                                                        \
	{                                                                                                         \
		const uint _rgb_buffer_base = RGB_BUFFER_BASE(pixel_idx);                                            \
		const vec3 _rgb_buffer_value = (value);                                                               \
		atomicAdd(DEREF(field)[_rgb_buffer_base + 0u], _rgb_buffer_value.x);                                 \
		atomicAdd(DEREF(field)[_rgb_buffer_base + 1u], _rgb_buffer_value.y);                                 \
		atomicAdd(DEREF(field)[_rgb_buffer_base + 2u], _rgb_buffer_value.z);                                 \
	}

#endif
