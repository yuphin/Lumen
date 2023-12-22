#ifndef SAMPLING_COMMONS_GLSL
#define SAMPLING_COMMONS_GLSL
#include "../utils.glsl"

#define SAMPLING_MODE_COS_WEIGHTED 0
#define SAMPLING_MODE_CONCENTRIC_DISK_MAPPING 1

#define SAMPLING_MODE 1

vec3 sample_hemisphere(vec2 xi, out float phi) {
	phi = TWO_PI * xi.x;
	float cos_theta = (2.0 * xi.y - 1.0);
#if SAMPLING_MODE == SAMPLING_MODE_CONCENTRIC_DISK_MAPPING
	vec2 d = concentric_sample_disk(xi);
	float z = sqrt(max(0., 1. - dot(d, d)));
	return vec3(d, z);
#else
	return normalize(vec3(sqrt(1.0 - cos_theta * cos_theta) * vec2(cos(phi), sin(phi)), 1 - cos_theta));
#endif
}

vec3 sample_hemisphere(vec2 xi) {
	float unused_phi;
	return sample_hemisphere(xi, unused_phi);
}

#endif