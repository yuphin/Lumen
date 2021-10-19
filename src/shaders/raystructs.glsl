#define PI 3.14159265359
#define PI2 6.28318530718
struct HitPayload
{
  vec3 hit_value;
  vec3 geometry_nrm;
  vec3 shading_nrm;
  vec3 pos;
  vec2 uv;
  uint material_idx;
};
