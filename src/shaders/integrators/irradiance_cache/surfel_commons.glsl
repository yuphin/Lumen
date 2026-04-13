void surfel_clear(out Surfel surfel) { surfel.radius = 0; }

bool surfel_valid(Surfel surfel) { return surfel.radius != 0; }

float surfel_radius_factor(float desired_surfel_radius_px) { return 2.0 * desired_surfel_radius_px / (ubo.projection[1][1] * pc.height); }

float surfel_px_size(Surfel surfel, vec4 view_pos) {
	float px_size = ubo.projection[1][1] * pc.height * surfel.radius / (2.0 * view_pos.z);
	return px_size;
}