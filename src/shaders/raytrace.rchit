#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "utils.glsl"
#include "commons.h"

hitAttributeEXT vec2 attribs;

layout(location = 0) rayPayloadInEXT HitPayload payload;
layout(location = 1) rayPayloadEXT bool is_shadowed;

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 0, binding = 2) readonly buffer InstanceInfo_ {PrimMeshInfo prim_info[];};
layout(set = 1, binding = 1, scalar) buffer SceneDesc_ { SceneDesc scene_desc; };
layout(set = 1, binding = 2) uniform sampler2D textures[];
layout(buffer_reference, scalar) readonly buffer Vertices {vec3 v[]; };
layout(buffer_reference, scalar) readonly buffer Indices {uint i[]; };
layout(buffer_reference, scalar) readonly buffer Normals {vec3 n[]; };
layout(buffer_reference, scalar) readonly buffer TexCoords {vec2 t[]; };
layout(buffer_reference, scalar) readonly buffer Materials {GLTFMaterial m[]; };
layout(push_constant) uniform _PushConstantRay { PushConstantRay pc_ray; };


vec3 compute_diffuse(GLTFMaterial mat, vec3 light_dir, vec3 normal)
{
  // Lambertian
  float dot_nl = max(dot(normal, light_dir), 0.0);
  vec3  c     = vec3(mat.base_color_factor) * dot_nl;
  return c;
}

void main() {
  PrimMeshInfo pinfo = prim_info[gl_InstanceCustomIndexEXT];

   // Getting the 'first index' for this mesh (offset of the mesh + offset of the triangle)
  uint index_offset  = pinfo.index_offset + 3 * gl_PrimitiveID;
  uint vertex_offset = pinfo.vertex_offset;           // Vertex offset as defined in glTF
  uint material_index     = max(0, pinfo.material_index);  // material of primitive mesh

  // Object data
  Materials materials = Materials(scene_desc.material_addr);
  Indices indices = Indices(scene_desc.index_addr);
  Vertices vertices = Vertices(scene_desc.vertex_addr);
  Normals normals = Normals(scene_desc.normal_addr);
  TexCoords tex_coords = TexCoords(scene_desc.uv_addr);

  // Indices of the triangle
  ivec3 ind = ivec3(indices.i[index_offset + 0], indices.i[index_offset + 1], indices.i[index_offset + 2]);
  
  ind +=  ivec3(vertex_offset);
  // Vertex of the triangle
  const vec3 v0 = vertices.v[ind.x];
  const vec3 v1 = vertices.v[ind.y];
  const vec3 v2 = vertices.v[ind.z];

  const vec3 n0 = normals.n[ind.x];
  const vec3 n1 = normals.n[ind.y];
  const vec3 n2 = normals.n[ind.z];

  const vec2 uv0 = tex_coords.t[ind.x];
  const vec2 uv1 = tex_coords.t[ind.y];
  const vec2 uv2 = tex_coords.t[ind.z];
  const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
  // Computing the coordinates of the hit position
  const vec3 pos      = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
  const vec3 world_pos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));  // Transforming the position to world space

  // Computing the normal at hit position
  const vec3 nrm      = normalize(n0 * barycentrics.x + n1 * barycentrics.y + n2 * barycentrics.z);
  //const vec3 world_nrm = vec3(normalize(gl_ObjectToWorldEXT * vec4(nrm,0.0)));  // Transforming the normal to world space
  const vec3 world_nrm = vec3(normalize(gl_ObjectToWorldEXT * vec4(nrm,0.0)));  // Transforming the normal to world space

  const vec2 uv = uv0 * barycentrics.x + uv1 * barycentrics.y + uv2 * barycentrics.z;

  // Vector toward the light
  vec3  L;
  float light_intensity = pc_ray.light_intensity;
  float light_distance  = 250;
  // Point light
  if(pc_ray.light_type == 0) {
    vec3 ldir = pc_ray.light_pos - world_pos;
    light_distance = length(ldir);
    light_intensity = pc_ray.light_intensity / (light_distance * light_distance);
    L = normalize(ldir);
  }
  else {  // Directional light
    L = normalize(pc_ray.light_pos);
  }
  // Material of the object
  GLTFMaterial mat = materials.m[material_index];
  // Diffuse
  vec3 diffuse = compute_diffuse(mat, L, world_nrm);
  if(mat.texture_id > -1) {
    diffuse *= texture(textures[mat.texture_id], uv).xyz;
  }
  vec3  specular    = vec3(0);
  float attenuation = 1;
  // Tracing shadow ray only if the light is visible from the surface
  if(dot(world_nrm, L) > 0) {
    float tmin = 0.001;
    float tmax = light_distance;
    vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 ray_dir = L;
    uint flags  = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    is_shadowed   = true;
    traceRayEXT(tlas,  // acceleration structure
                flags,       // rayFlags
                0xFF,        // cullMask
                0,           // sbtRecordOffset
                0,           // sbtRecordStride
                1,           // missIndex
                origin,      // ray origin
                tmin,        // ray min range
                ray_dir,      // ray direction
                tmax,        // ray max range
                1            // payload (location = 1)
    );
    if(is_shadowed) {
      attenuation = 0.3;
    }
  }
  payload.hit_value = vec3(light_intensity * attenuation * (diffuse + specular));
}