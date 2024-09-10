#include "../../utils.glsl"

#define IRR_PROBE_SIDE_LENGTH 8
#define IRR_PROBE_WITH_BORDER 10

#define DEPTH_PROBE_SIDE_LENGTH 16
#define DEPTH_PROBE_WITH_BORDER 18
ivec3 probe_coord_from_pos(vec3 p) {
	return clamp(ivec3((p - ddgi_uniforms.probe_start_position) / ddgi_uniforms.probe_step), ivec3(0, 0, 0),
				 ivec3(ddgi_uniforms.probe_counts) - ivec3(1, 1, 1));
}

int probe_coord_to_idx(ivec3 probe_coords) {
	return int(probe_coords.x + probe_coords.y * ddgi_uniforms.probe_counts.x +
			   probe_coords.z * ddgi_uniforms.probe_counts.x * ddgi_uniforms.probe_counts.y);
}

vec3 probe_coord_to_pos(ivec3 probe_coord) {
	return ddgi_uniforms.probe_step * vec3(probe_coord) + ddgi_uniforms.probe_start_position;
}

float pow3(float v) { return sqr(v) * v; };


vec2 texture_coord_from_direction_irr(vec3 dir, int probe_index, int full_texture_width, int full_texture_height) {
	vec2 normalized_oct_coord = oct_encode(normalize(dir));
	int probes_per_row = ddgi_uniforms.probe_counts.x * ddgi_uniforms.probe_counts.y;
	vec2 probe_top_left_coords = vec2((probe_index % probes_per_row) * IRR_PROBE_WITH_BORDER,
									  (probe_index / probes_per_row) * IRR_PROBE_WITH_BORDER);
	return (probe_top_left_coords + 0.5 * IRR_PROBE_WITH_BORDER + 0.5 * IRR_PROBE_SIDE_LENGTH * normalized_oct_coord) /
		   vec2(full_texture_width, full_texture_height);
}

vec2 texture_coord_from_direction_depth(vec3 dir, int probe_index, int full_texture_width, int full_texture_height) {
	vec2 normalized_oct_coord = oct_encode(normalize(dir));
	int probes_per_row = ddgi_uniforms.probe_counts.x * ddgi_uniforms.probe_counts.y;
	vec2 probe_top_left_coords = vec2((probe_index % probes_per_row) * DEPTH_PROBE_WITH_BORDER,
									  (probe_index / probes_per_row) * DEPTH_PROBE_WITH_BORDER);
	return (probe_top_left_coords + 0.5 * DEPTH_PROBE_WITH_BORDER +
			0.5 * DEPTH_PROBE_SIDE_LENGTH * normalized_oct_coord) /
		   vec2(full_texture_width, full_texture_height);
}



vec3 sample_irradiance(vec3 p, vec3 n, vec3 wo) {
	vec3 p_orig = p;

#if 1
	const vec3 bias_vec = (n * 0.2 + wo * 0.8) * (0.75 * ddgi_uniforms.probe_step) * ddgi_uniforms.normal_bias;
#else
	const vec3 bias_vec = (n + 3.0 * wo) * ddgi_uniforms.normal_bias;
#endif
	p = p + bias_vec;
	const ivec3 probe_coord = probe_coord_from_pos(p);
	const vec3 probe_pos = probe_coord_to_pos(probe_coord);
	const float energy_conservation = 0.85;
	vec3 sum_irradiance = vec3(0.0);
	float sum_weight = 0.0;
	vec3 alpha = clamp((p - probe_pos) / ddgi_uniforms.probe_step, vec3(0.0), vec3(1.0));
	for (int i = 0; i < 8; i++) {
		ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);
		ivec3 offseted_probe_coord = clamp(probe_coord + offset, ivec3(0), ddgi_uniforms.probe_counts - ivec3(1));
		int offseted_idx = probe_coord_to_idx(offseted_probe_coord);
		float probe_state = probe_offsets.d[offseted_idx].w;
		if (probe_state == DDGI_PROBE_INACTIVE) {
			continue;
		}
		vec3 offseted_probe_pos = probe_offsets.d[offseted_idx].xyz + probe_coord_to_pos(offseted_probe_coord);

		vec3 offseted_probe_to_p = p - offseted_probe_pos;
		vec3 dir = normalize(offseted_probe_to_p);
		vec3 trilinear = mix(1.0 - alpha, alpha, offset);
		float weight = 1.0;
		vec3 true_dir_to_offseted_probe = normalize(offseted_probe_pos - p_orig);
		// Smooth backface
		weight *= sqr(max(0.0001, (dot(true_dir_to_offseted_probe, n) + 1.0) * 0.5)) + 0.2;
		// Moment visibility

		vec2 tex_coord = texture_coord_from_direction_depth(dir, offseted_idx, ddgi_uniforms.depth_width,
															ddgi_uniforms.depth_height);
		float dist_to_probe = length(offseted_probe_to_p);
		vec2 temp = textureLod(depth_img, tex_coord, 0.0f).rg;
		float mean = temp.x;
		float variance = abs(sqr(temp.x) - temp.y);
		float chebyshev_weight = variance / (variance + sqr(max(dist_to_probe - mean, 0.0)));

		chebyshev_weight = max(pow3(chebyshev_weight), 0.0);

		weight *= (dist_to_probe <= mean) ? 1.0 : chebyshev_weight;
		weight = max(0.000001, weight);
		// Get irradiance
		tex_coord = texture_coord_from_direction_irr(normalize(n), offseted_idx, ddgi_uniforms.irradiance_width,
													 ddgi_uniforms.irradiance_height);
		vec3 offseted_probe_irradiance = textureLod(irr_img, tex_coord, 0.0f).rgb;
		const float crush_threshold = 0.2f;
		if (weight < crush_threshold) {
			weight *= weight * weight * (1.0f / sqr(crush_threshold));
		}
		// Trilinear weights
		weight *= trilinear.x * trilinear.y * trilinear.z;
		sum_irradiance += weight * offseted_probe_irradiance;
		sum_weight += weight;
	}
	vec3 net_irradiance = sum_irradiance * energy_conservation / sum_weight;
	return PI * net_irradiance;
}