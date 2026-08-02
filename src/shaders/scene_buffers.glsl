#ifndef SCENE_BUFFERS_GLSL
#define SCENE_BUFFERS_GLSL
#include "bda.glsl"

// Requires a SceneDesc scene_desc binding and the types from commons.h
SCENE_BUFFER_RO(prim_info, PrimInfo);
SCENE_BUFFER_RO(material, Material);
SCENE_BUFFER_RO(index, uint);
SCENE_BUFFER_RO(compact_vertices, Vertex);
SCENE_BUFFER_RO(light_triangle_cdf, LightTriangleCDF);
SCENE_BUFFER_RO(emitter_light_idx, uint);
SCENE_BUFFER_RO(transformations, InstanceTransform);

#endif
