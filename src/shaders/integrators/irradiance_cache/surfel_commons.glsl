float surfel_radius_factor(float desired_surfel_radius_px) {
	return 2.0 * desired_surfel_radius_px / (ubo.projection[1][1] * pc.height);
}

float surfel_dist_from_camera(vec3 world_pos) {
	vec3 cam_pos = (ubo.inv_view * vec4(0, 0, 0, 1)).xyz;
	return max(length(world_pos - cam_pos), 1e-6);
}

float surfel_radius_for_position(vec3 world_pos) {
	float dist;
	if (pc.use_camera_relative_surfel_size > 0) {
		dist = surfel_dist_from_camera(world_pos);
	} else {
		vec4 view_pos = ubo.view * vec4(world_pos, 1.0);
		dist = view_pos.z;
	}
	return abs(surfel_radius_factor(pc.desired_surfel_radius_px) * dist);
}

float surfel_radius_px_size_for_position(Surfel surfel, vec3 world_pos, vec4 view_pos) {
	float dist = pc.use_camera_relative_surfel_size > 0 ? surfel_dist_from_camera(world_pos) : view_pos.z;
	return abs(ubo.projection[1][1] * pc.height * surfel.radius / (2.0 * dist));
}

float surfel_reconstruction_weight(vec3 from_surfel_center, vec3 normal, vec3 surfel_normal, float radius) {
	float n_dot = dot(normal, surfel_normal);
	if (n_dot < 0.3) {
		return 0.0;
	}

	float dist = length(from_surfel_center);
	float dist_normal = dot(from_surfel_center, surfel_normal);
	float dist_tangent = sqrt(max(0.0, dist * dist - dist_normal * dist_normal));

	// Penalize normal distance
	float dist_aniso = sqrt(dist_tangent * dist_tangent + 4.0 * dist_normal * dist_normal);
	float ratio = dist_aniso / (2.0f * radius);
	float one_minus_ratio_sqr = (1.0 - ratio) * (1.0 - ratio);
	float one_minus_ratio_sqr_sqr = one_minus_ratio_sqr * one_minus_ratio_sqr;
	float weight_pos = ratio >= 1.0 ? 0.0 : one_minus_ratio_sqr_sqr * (1.0 + 4.0 * ratio);
	float weight_normal = pow(max(n_dot, 0.0), 4.0);
	return weight_pos * weight_normal;
}
