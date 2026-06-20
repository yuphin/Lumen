#ifndef IR_DIRECT_LIGHTING_GLSL
#define IR_DIRECT_LIGHTING_GLSL

vec3 uniform_sample_light(inout uvec4 seed, vec3 pos, const vec3 n_s, vec3 wo, Material hit_material,
						  bool forward_facing) {
	if (pc.num_lights == 0 || pc.total_light_count == 0) {
		return vec3(0);
	}

	vec3 wi;
	float wi_len;
	float pdf_light_w;
	float pdf_light_a;
	LightRecord record;
	float cos_from_light;
	const vec3 Le =
		sample_Li(rand4(seed), pos, pc.num_lights, pdf_light_w, wi, wi_len, pdf_light_a, cos_from_light, record);

	if (wi_len <= EPS || pdf_light_w <= 0.0) {
		return vec3(0);
	}

	const vec3 p = offset_ray2(pos, n_s);
	const float cos_x = dot(n_s, wi);
	const vec3 f = eval_bsdf(hit_material, wo, wi, n_s, /*mode=*/1, /*forward_facing=*/forward_facing);

	any_hit_payload.hit = 1;
	traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0x1, 1, 0, 1, p, 0, wi,
				wi_len - EPS, 1);

	const float light_pick_pdf = 1.0 / float(pc.total_light_count);
	if (any_hit_payload.hit == 0) {
		return f * abs(cos_x) * Le / (pdf_light_w * light_pick_pdf);
	}
	return vec3(0);
}

#endif
