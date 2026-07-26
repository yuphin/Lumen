#ifndef SHADOW_RAY_GLSL
#define SHADOW_RAY_GLSL

// Connections shorter than EPS would trace with a negative tmax, which Vulkan leaves undefined.
// Report them as occluded instead.
bool connection_occluded(vec3 origin, vec3 dir, float dist, uint cull_mask) {
	if (dist <= EPS) {
		return true;
	}
	any_hit_payload.hit = 1;
	traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, cull_mask, 1, 0, 1,
				origin, 0, dir, dist - EPS, 1);
	return any_hit_payload.hit != 0;
}

#endif