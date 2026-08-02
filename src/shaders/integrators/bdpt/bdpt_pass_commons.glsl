#ifndef BDPT_PASS_COMMONS_GLSL
#define BDPT_PASS_COMMONS_GLSL

#include "../../bda.glsl"
#include "../../atomic_rgb.glsl"
#include "bdpt_commons.h"
#include "../../commons.glsl"

layout(location = 0) rayPayloadEXT HitPayload payload;
layout(location = 1) rayPayloadEXT AnyHitPayload any_hit_payload;
layout(push_constant) uniform _PushConstantRay { PCBDPT pc; };

SCENE_BUFFER(light_path, PathVertex);
SCENE_BUFFER(camera_path, PathVertex);
SCENE_BUFFER(path_cnt, BDPTPathCounts);
SCENE_BUFFER(color_storage, float);

uint screen_size = gl_LaunchSizeEXT.x * gl_LaunchSizeEXT.y;
uint pixel_idx = gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y;
uint bdpt_path_idx = pixel_idx * (pc.max_depth + 1);

const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;
#define RR_MIN_DEPTH 3

uvec4 seed = init_rng(gl_LaunchIDEXT.xy, gl_LaunchSizeEXT.xy,
					  pc.frame_num ^ pc.time, BDPT_RNG_STREAM_SALT);

#include "../bdpt_commons.glsl"

#endif
