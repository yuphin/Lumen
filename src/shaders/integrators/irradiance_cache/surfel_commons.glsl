void surfel_clear(out Surfel surfel) { surfel.radius = 0; }

bool surfel_valid(Surfel surfel) { return surfel.radius != 0; }

float surfel_radius_factor() { return 2.0 * pc.desired_surfel_radius_px / (ubo.projection[1][1] * pc.height); }