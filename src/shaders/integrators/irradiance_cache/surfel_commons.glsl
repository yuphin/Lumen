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
