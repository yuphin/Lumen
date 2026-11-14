#include "../../utils.glsl"
#include "../../surface.glsl"

bool unpack_pixel_geometry(PixelGeometry geometry, ivec2 pixel, uvec2 size, out vec3 pos, out vec3 n) {
	if (geometry.t <= 0.0) {
		return false;
	}
	vec2 ndc = (vec2(pixel) + 0.5) / vec2(size) * 2.0 - 1.0;
	vec4 target = ubo.inv_projection * vec4(ndc, 1, 1);
	vec3 dir = normalize((ubo.inv_view * vec4(normalize(target.xyz), 0)).xyz);
	pos = ubo.view_pos.xyz + geometry.t * dir;
	n = unpack_normal_octahedral(geometry.normal);
	return true;
}
