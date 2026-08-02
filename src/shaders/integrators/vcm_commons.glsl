#ifndef VCM_COMMONS
#define VCM_COMMONS

#include "../shadow_ray.glsl"

#ifndef VC_MLT
#define VC_MLT 0
#endif
#ifndef VCM_MLT
#define VCM_MLT 0
#endif

#if VC_MLT == 1
#include "mlt_commons.glsl"
#elif VCM_MLT == 1
#include "mlt_commons_vcmmlt.glsl"
#endif

vec3 normalize_grid(vec3 p, vec3 min_bnds, vec3 max_bnds) { return (p - min_bnds) / (max_bnds - min_bnds); }

ivec3 get_grid_idx(vec3 p, vec3 min_bnds, vec3 max_bnds, ivec3 grid_res) {
	ivec3 res = ivec3(normalize_grid(p, min_bnds, max_bnds) * grid_res);
	clamp(res, vec3(0), grid_res - vec3(1));
	return res;
}
vec3 vcm_connect_cam(const vec3 cam_pos, const vec3 cam_nrm, const vec3 n_s, const vec3 n_g, const float cam_A,
                     const vec3 pos, const in VCMState state, const float eta_vm, const vec3 wo,
                     const Material mat, out ivec2 coords) {
	vec3 L = vec3(0);
	vec3 dir = cam_pos - pos;
	float len = length(dir);
	dir /= len;
	float cos_y = dot(dir, n_g);
	float cos_theta = dot(cam_nrm, -dir);
	if (cos_theta <= 0.) {
		return L;
	}

	// if(dot(n_s, dir) < 0) {
	//     n_s *= -1;
	// }
	// pdf_rev / pdf_fwd
	// in the case of light coming to camera
	// simplifies to abs(cos(theta)) / (A * cos^3(theta) * len^2)
	float cos_3_theta = cos_theta * cos_theta * cos_theta;
	const float cam_pdf_ratio = abs(cos_y) / (cam_A * cos_3_theta * len * len);
	vec3 ray_origin = offset_ray2(pos, n_g, dot(dir, n_g) < 0.0);
	float pdf_rev, pdf_fwd;
	const vec3 f = eval_bsdf(n_s, n_g, wo, mat, TRANSPORT_MODE_FROM_LIGHT,
						   dot(payload.n_s, wo) > 0, dir, pdf_fwd, pdf_rev);
	if (f == vec3(0)) {
		return L;
	}
	if (cam_pdf_ratio > 0.0) {
		if (!connection_occluded(ray_origin, dir, len, 0xFF)) {
			const float w_light = (cam_pdf_ratio / (screen_size)) * (eta_vm + state.d_vcm + pdf_rev * state.d_vc);

			const float mis_weight = 1. / (1. + w_light);
			// We / pdf_we * abs(cos_theta) = cam_pdf_ratio
			L = mis_weight * state.throughput * cam_pdf_ratio * f / screen_size;
			// if(isnan(luminance(L))) {
			//     debugPrintfEXT("%v3f\n", state.throughput);
			// }
		}
	}
	dir = -dir;
	vec4 target = ubo.view * vec4(dir.x, dir.y, dir.z, 0);
	target /= target.z;
	target = -ubo.projection * target;
	vec2 screen_dims = vec2(pc.width, pc.height);
	coords = ivec2(0.5 * (1 + target.xy) * screen_dims - 0.5);
	if (coords.x < 0 || coords.x >= pc.width || coords.y < 0 || coords.y >= pc.height || dot(dir, cam_nrm) < 0) {
		return vec3(0);
	}
	return L;
}

bool vcm_generate_light_sample(float eta_vc, out VCMState light_state, out bool finite) {
#if VC_MLT == 1 || VCM_MLT == 1
	const vec4 rands_pos = vec4(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step),
								mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
	const vec2 rands_dir = vec2(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
#else
	const vec4 rands_pos = rand4(seed);
	const vec2 rands_dir = rand2(seed);
#endif
	const LightLeSample light_sample = sample_light_Le(rands_pos, rands_dir, pc.num_lights);
	if (light_sample.pdf_joint <= 0.0) {
		return false;
	}

	light_state.pos = light_sample.position;
	light_state.area = 1.0 / light_sample.pdf_position_a;
	light_state.wi = light_sample.wi;
	light_state.throughput =
		light_sample.Le * light_sample.cos_from_light / light_sample.pdf_joint;

	// Partially evaluate pdfs (area formulation)
	// At s = 0 this is p_rev / p_fwd, in the case of area lights:
	// p_rev = p_connect = 1/area, p_fwd = cos_theta / (PI * area)
	// Note that pdf_fwd is in area formulation, so cos_y / r^2 is missing
	// currently.
	light_state.d_vcm = 1.0 / light_sample.pdf_direction_w;
	// g_prev / p_fwd
	// Note that g_prev component in d_vc and d_vm lags by 1 iter
	// So we initialize g_prev to cos_theta of the current iter
	// Also note that 1/r^2 in the geometry term cancels for vc and vm
	// By convention pdf_fwd sample the i'th vertex from i-1
	// g_prev or pdf_prev samples from i'th vertex to i-1
	// In that sense, cos_theta terms will be common in g_prev and pdf_pwd
	// Similar argument, with the eta
	finite = is_light_finite(light_sample.flags);
	if (!is_light_delta(light_sample.flags)) {
		light_state.d_vc = (finite ? light_sample.cos_from_light : 1.0) /
						   light_sample.pdf_joint;
	} else {
		light_state.d_vc = 0;
	}
	light_state.d_vm = light_state.d_vc * eta_vc;
	return true;
}

vec3 vcm_get_light_radiance(in const Material mat, in const VCMState camera_state, int d) {
	const uint light_idx = light_index_from_primitive(payload.instance_idx);
	if (light_idx == INVALID_LIGHT_INDEX) {
		return vec3(0.0);
	}
	const Light light = lights[light_idx];
	const TriangleRecord emitter_triangle = triangle_at_bary(
		DEREF(prim_info)[light.prim_mesh_idx], vec2(0.0), payload.triangle_idx, light.world_matrix);
	const float cos_from_light = dot(emitter_triangle.n_g, -camera_state.wi);
	const vec3 Le = (is_light_two_sided(light.light_flags) || cos_from_light > 0.0)
						? mat.emissive_factor
						: vec3(0.0);
	if (d == 1) {
		return Le;
	}
	const float pdf_light_pos = light_emission_position_pdf_a(light_idx, pc.num_lights);
	const float pdf_light_dir =
		light_emission_direction_pdf_w(light_idx, emitter_triangle.n_g, -camera_state.wi);
	const float w_camera = pdf_light_pos * camera_state.d_vcm +
						   (pc.use_vc == 1 || pc.use_vm == 1 ? (pdf_light_pos * pdf_light_dir) * camera_state.d_vc : 0);
	const float mis_weight = 1. / (1. + w_camera);
	return mis_weight * Le;
}

vec3 vcm_connect_light(const vec3 n_s, const vec3 n_g, const vec3 wo, const Material mat, const bool side,
					   const float eta_vm, const VCMState camera_state) {
	LightLiSample light_sample;
	vec3 res = vec3(0.0);
#if VC_MLT == 1
	const vec4 rands_pos = vec4(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step),
								mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
	light_sample = sample_light_Li(rands_pos, payload.pos, pc.num_lights);
#elif VCM_MLT == 1
	if (SEEDING == 1) {
		light_sample = sample_light_Li(rand4(seed), payload.pos, pc.num_lights);
	} else {
		const vec4 rands_pos = vec4(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step),
									mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
		light_sample = sample_light_Li(rands_pos, payload.pos, pc.num_lights);
	}
#else
	light_sample = sample_light_Li(rand4(seed), payload.pos, pc.num_lights);
#endif

	const float cos_x_s = dot(light_sample.wi, n_s);
	const float cos_x_g = dot(light_sample.wi, n_g);
	const vec3 ray_origin = offset_ray2(payload.pos, n_g, dot(light_sample.wi, n_g) < 0.0);
	float pdf_fwd, pdf_rev;
	const vec3 f =
		eval_bsdf(n_s, n_g, wo, mat, TRANSPORT_MODE_FROM_CAMERA, side, light_sample.wi, pdf_fwd, pdf_rev);
	if (f != vec3(0)) {
		// Vertex is on the emitter
		const bool visible = !connection_occluded(ray_origin, light_sample.wi, light_sample.distance, 0xFF);
		if (visible) {
			if (is_light_delta(light_sample.flags)) {
				pdf_fwd = 0;
			}
			const float emission_pdf = light_emission_pdf(
				light_sample.identity.light_idx, light_sample.normal, -light_sample.wi, pc.num_lights);
			const float cos_y = abs(dot(light_sample.normal, -light_sample.wi));
			const float w_light = pdf_fwd / light_sample.pdf_position_w;
			const float w_cam = emission_pdf * abs(cos_x_g) / (light_sample.pdf_position_w * cos_y) *
								(eta_vm + camera_state.d_vcm + camera_state.d_vc * pdf_rev);
			const float mis_weight = 1. / (1. + w_light + w_cam);
			if (mis_weight > 0) {
				res = mis_weight * abs(cos_x_s) * f * camera_state.throughput *
					  light_sample.Li / light_sample.pdf_position_w;
			}
		}
	}
	return res;
}

#define light_vtx(i) DEREF(vcm_vertices)[i]
vec3 vcm_connect_light_vertices(uint light_path_len, uint light_path_idx, int depth, const vec3 n_s,
								const vec3 n_g, const vec3 wo, const Material mat, const bool side,
								const float eta_vm, const VCMState camera_state) {
	vec3 res = vec3(0);
	for (int i = 0; i < light_path_len; i++) {
		uint s = light_vtx(light_path_idx + i).path_len;
		uint mdepth = s + depth - 1;
		if (mdepth >= pc.max_depth) {
			break;
		}
		vec3 dir = light_vtx(light_path_idx + i).pos - payload.pos;
		const float len = length(dir);
		const float len_sqr = len * len;
		dir /= len;
		const VCMVertex light_vertex = light_vtx(light_path_idx + i);
		const vec3 light_n_g = unpack_normal_octahedral(light_vertex.packed_n_g);
		const float cos_cam = dot(n_g, dir);
		const float cos_light = dot(light_n_g, -dir);
		const float G = abs(cos_light * cos_cam) / len_sqr;
		if (G > 0) {
			float cam_pdf_fwd, cam_pdf_rev, light_pdf_fwd, light_pdf_rev;
			const vec3 f_cam = eval_bsdf(n_s, n_g, wo, mat, TRANSPORT_MODE_FROM_CAMERA, side, dir, cam_pdf_fwd,
										 cam_pdf_rev);
			const Material light_mat =
				load_material(light_vertex.material_idx, light_vertex.uv);
			// TODO: what about anisotropic BSDFS?
			const vec3 f_light = eval_bsdf(light_vertex.n_s, light_n_g, light_vertex.wo, light_mat,
										 TRANSPORT_MODE_FROM_LIGHT, light_vertex.side == 1, -dir,
										 light_pdf_fwd, light_pdf_rev);
			if (f_light != vec3(0) && f_cam != vec3(0)) {
				cam_pdf_fwd *= abs(cos_light) / len_sqr;
				light_pdf_fwd *= abs(cos_cam) / len_sqr;
				const float light_factor = eta_vm + light_vertex.d_vcm + light_pdf_rev * light_vertex.d_vc;
				const float cam_factor = eta_vm + camera_state.d_vcm + cam_pdf_rev * camera_state.d_vc;
				const float w_light = light_factor == 0.0 ? 0.0 : cam_pdf_fwd * light_factor;
				const float w_camera = cam_factor == 0.0 ? 0.0 : light_pdf_fwd * cam_factor;
				const float mis_weight = 1. / (1 + w_camera + w_light);
				if (!(mis_weight > 0)) {
					continue;
				}
				const vec3 ray_origin = offset_ray2(payload.pos, n_g, dot(dir, n_g) < 0.0);
				const bool visible = !connection_occluded(ray_origin, dir, len, 0xFF);
				if (visible) {
					res += mis_weight * G * camera_state.throughput * light_vertex.throughput * f_cam *
						  f_light;
				}
			}
		}
	}
	return res;
}
#undef light_vtx

#if VC_MLT == 0
vec3 vcm_merge_light_vertices(uint light_path_len, uint light_path_idx, int depth, vec3 n_s, vec3 n_g, vec3 wo,
							  Material mat, bool side, float eta_vc, VCMState camera_state, float radius,
							  float normalization_factor) {
	float r_sqr = radius * radius;
	vec3 res = vec3(0);
	ivec3 grid_min_bnds_idx = get_grid_idx(payload.pos - vec3(radius), pc.min_bounds, pc.max_bounds, pc.grid_res);
	ivec3 grid_max_bnds_idx = get_grid_idx(payload.pos + vec3(radius), pc.min_bounds, pc.max_bounds, pc.grid_res);
	for (int x = grid_min_bnds_idx.x; x <= grid_max_bnds_idx.x; x++) {
		for (int y = grid_min_bnds_idx.y; y <= grid_max_bnds_idx.y; y++) {
			for (int z = grid_min_bnds_idx.z; z <= grid_max_bnds_idx.z; z++) {
				const uint h = hash(ivec3(x, y, z), screen_size);
				if (DEREF(photon)[h].photon_count > 0) {
					const vec3 pp = payload.pos - DEREF(photon)[h].pos;
					const float dist_sqr = dot(pp, pp);
					if (dist_sqr > r_sqr) {
						continue;
					}
					// Should we?
					uint depth = DEREF(photon)[h].path_len + depth - 1;
					if (depth > pc.max_depth) {
						continue;
					}
					float cam_pdf_fwd, cam_pdf_rev;
					const float cos_theta = dot(DEREF(photon)[h].wi, n_s);
					vec3 f = eval_bsdf(n_s, n_g, wo, mat, TRANSPORT_MODE_FROM_CAMERA, side, DEREF(photon)[h].wi,
									  cam_pdf_fwd, cam_pdf_rev);

					if (f != vec3(0)) {
						const float photon_vcm = DEREF(photon)[h].d_vcm;
						const float photon_vm = DEREF(photon)[h].d_vm;
						const float w_light = (photon_vcm == 0.0 ? 0.0 : photon_vcm * eta_vc) +
											  (photon_vm == 0.0 ? 0.0 : photon_vm * cam_pdf_fwd);
						const float w_cam = (camera_state.d_vcm == 0.0 ? 0.0 : camera_state.d_vcm * eta_vc) +
											(camera_state.d_vm == 0.0 ? 0.0 : camera_state.d_vm * cam_pdf_rev);

						const float mis_weight = 1. / (1 + w_light + w_cam);
						if (!(mis_weight > 0)) {
							continue;
						}
						float cos_nrm = dot(DEREF(photon)[h].nrm, n_s);
						if (cos_nrm > EPS) {
							const float w = 1. - sqrt(dist_sqr) / radius;
							const float w_normalization = 3.;  // 1. / (1 - 2/(3*k)) where k =
															   // 1
							res += w * mis_weight * DEREF(photon)[h].photon_count * DEREF(photon)[h].throughput * f *
								   camera_state.throughput * normalization_factor * w_normalization;
						}
					}
				}
			}
		}
	}
	return res;
}
#endif

#if VC_MLT == 1 || VCM_MLT == 1
float vcm_fill_light(vec3 origin, VCMState vcm_state, bool finite_light,
#else
void vcm_fill_light(vec3 origin, VCMState vcm_state, bool finite_light,
#endif
					 float eta_vcm, float eta_vc, float eta_vm) {
#define light_vtx(i) DEREF(vcm_vertices)[vcm_light_path_idx + i]
	const vec3 cam_pos = origin;
	const vec3 cam_nrm = vec3(-ubo.inv_view * vec4(0, 0, 1, 0));
	const float radius = pc.radius;
	const float radius_sqr = radius * radius;
	vec4 area_int = (ubo.inv_projection * vec4(2. / gl_LaunchSizeEXT.x, 2. / gl_LaunchSizeEXT.y, 0, 1));
	area_int /= (area_int.w);
	const float cam_area = abs(area_int.x * area_int.y);
	int depth;
	int path_idx = 0;
	bool specular = false;
#if VC_MLT == 1 || VCM_MLT == 1
	float lum_sum = 0;
#endif
	light_vtx(path_idx).path_len = 0;
	for (depth = 1;; depth++) {
		traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, vcm_state.pos, tmin, vcm_state.wi, tmax, 0);
		if (payload.material_idx == -1) {
			break;
		}
		vec3 wo = vcm_state.pos - payload.pos;

		vec3 n_s = payload.n_s;
		vec3 n_g = payload.n_g;
		bool side = true;
		if (dot(payload.n_g, wo) <= 0.) n_g = -n_g;
		if (dot(n_g, n_s) < 0) {
			n_s = -n_s;
			side = false;
		}
		float cos_wo = dot(wo, n_s);
		float dist = length(payload.pos - vcm_state.pos);
		float dist_sqr = dist * dist;
		wo /= dist;
		const Material mat = load_material(payload.material_idx, payload.uv);
		const bool mat_specular = (mat.bsdf_props & BSDF_FLAG_SPECULAR) == BSDF_FLAG_SPECULAR;
		// Complete the missing geometry terms
		const float cos_theta_wo = abs(dot(wo, n_g));

		if (depth > 1 || finite_light) {
			vcm_state.d_vcm *= dist_sqr;
		}
		vcm_state.d_vcm /= cos_theta_wo;
		vcm_state.d_vc /= cos_theta_wo;
		vcm_state.d_vm /= cos_theta_wo;
		if ((!mat_specular && (pc.use_vc == 1 || pc.use_vm == 1))) {
			// Copy to light vertex buffer
			light_vtx(path_idx).wi = vcm_state.wi;
			light_vtx(path_idx).wo = wo;  //-vcm_state.wi;
			light_vtx(path_idx).n_s = n_s;
			light_vtx(path_idx).packed_n_g = pack_normal_octahedral(n_g);
			light_vtx(path_idx).pos = payload.pos;
			light_vtx(path_idx).uv = payload.uv;
			light_vtx(path_idx).material_idx = payload.material_idx;
			light_vtx(path_idx).area = payload.area;
			light_vtx(path_idx).throughput = vcm_state.throughput;
			light_vtx(path_idx).d_vcm = vcm_state.d_vcm;
			light_vtx(path_idx).d_vc = vcm_state.d_vc;
			light_vtx(path_idx).d_vm = vcm_state.d_vm;
			light_vtx(path_idx).path_len = depth + 1;
			light_vtx(path_idx).side = uint(side);
			path_idx++;
		}
		if (depth >= pc.max_depth) {
			break;
		}
		// Reverse pdf in solid angle form, since we have geometry term
		// at the outer paranthesis
		if (!mat_specular && (pc.use_vc == 1 && depth < pc.max_depth)) {
			// Connect to camera
			ivec2 coords;
			vec3 splat_col = vcm_connect_cam(cam_pos, cam_nrm, n_s, n_g, cam_area, payload.pos, vcm_state,
										eta_vm, wo, mat, coords);
#if VC_MLT == 1
#define splat(i) DEREF(light_splats)[splat_idx + i]
			const float lum = luminance(splat_col);
			if (save_radiance && lum > 0) {
				DEREF(connected_lights)[pixel_idx]++;

				uint idx = coords.x * pc.height + coords.y;
				const uint splat_cnt = DEREF(light_splat_cnts)[pixel_idx];
				DEREF(light_splat_cnts)[pixel_idx]++;
				splat(splat_cnt).idx = idx;
				splat(splat_cnt).L = splat_col;
			} else if (lum > 0) {
				DEREF(connected_lights)[pixel_idx]++;
				lum_sum += lum;
			}
#undef splat

#elif VCM_MLT == 1
			const float lum = luminance(splat_col);
			if (lum > 0) {
				lum_sum += lum;
				uint idx = coords.x * pc.height + coords.y;
				DEREF(color_storage)[idx] += splat_col;
			}
#else
								 if (luminance(splat_col) > 0) {
									 uint idx = coords.x * gl_LaunchSizeEXT.y + coords.y;
									 DEREF(color_storage)[idx] += splat_col;
								 }
#endif
		}

		// Continue the walk
		float pdf_dir;
		float cos_theta;
#if VC_MLT == 1 || VCM_MLT == 1
		vec3 rands_dir =
			vec3(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
		const vec3 f = sample_bsdf(n_s, n_g, wo, mat, TRANSPORT_MODE_FROM_LIGHT, side, vcm_state.wi,
								 pdf_dir, cos_theta, rands_dir);
#else
		const vec3 f = sample_bsdf(n_s, n_g, wo, mat, TRANSPORT_MODE_FROM_LIGHT, side, vcm_state.wi,
								 pdf_dir, cos_theta, seed);
#endif

		const bool same_hemisphere = same_hemisphere(vcm_state.wi, wo, n_s);

		const bool mat_transmissive = (mat.bsdf_props & BSDF_FLAG_TRANSMISSION) == BSDF_FLAG_TRANSMISSION;
		if (f == vec3(0) || pdf_dir == 0 || (!same_hemisphere && !mat_transmissive)) {
			break;
		}
		float pdf_rev = pdf_dir;
		if (!mat_specular) {
			pdf_rev = bsdf_pdf(mat, n_s, n_g, vcm_state.wi, wo, side);
		}
		const float abs_cos_theta = abs(cos_theta);
		const float mis_cos_theta = abs(dot(vcm_state.wi, n_g));

		vcm_state.pos = offset_ray(payload.pos, n_g, dot(vcm_state.wi, n_g) < 0.0);
		// Note, same cancellations also occur here from now on
		// see _vcm_generate_light_sample_
		if (!mat_specular) {
			vcm_state.d_vc = (mis_cos_theta / pdf_dir) * (eta_vm + vcm_state.d_vcm + pdf_rev * vcm_state.d_vc);
			vcm_state.d_vm = (mis_cos_theta / pdf_dir) * (1 + vcm_state.d_vcm * eta_vc + pdf_rev * vcm_state.d_vm);
			vcm_state.d_vcm = 1.0 / pdf_dir;
		} else {
			// Specular pdf has value = inf, so d_vcm = 0;
			vcm_state.d_vcm = 0;
			// pdf_fwd = pdf_rev = delta -> cancels
			vcm_state.d_vc *= mis_cos_theta;
			vcm_state.d_vm *= mis_cos_theta;
			specular = true;
		}
		vcm_state.throughput *= f * abs_cos_theta / pdf_dir;
		vcm_state.n_s = n_s;
		vcm_state.area = payload.area;
		vcm_state.material_idx = payload.material_idx;
	}
	DEREF(path_cnt)[pixel_idx] = path_idx;
	// "Build" the hash grid
	// TODO: Add sorting later
#if VC_MLT == 0
	if (pc.use_vm == 1) {
		for (int i = 0; i < path_idx; i++) {
			ivec3 grid_idx = get_grid_idx(light_vtx(i).pos, pc.min_bounds, pc.max_bounds, pc.grid_res);
			uint h = hash(grid_idx, screen_size);
			DEREF(photon)[h].pos = light_vtx(i).pos;
			DEREF(photon)[h].wi = -light_vtx(i).wi;
			DEREF(photon)[h].d_vm = light_vtx(i).d_vm;
			DEREF(photon)[h].d_vcm = light_vtx(i).d_vcm;
			DEREF(photon)[h].throughput = light_vtx(i).throughput;
			DEREF(photon)[h].nrm = light_vtx(i).n_s;
			DEREF(photon)[h].path_len = light_vtx(i).path_len;
			atomicAdd(DEREF(photon)[h].photon_count, 1);
		}
	}
#endif
#if VC_MLT == 1 || VCM_MLT == 1
	return lum_sum;
#endif
}
#undef light_vtx

vec3 vcm_trace_eye(VCMState camera_state, float eta_vcm, float eta_vc,
#if VC_MLT == 1 || VCM_MLT == 1
				   float eta_vm, out float lum) {
#define splat(i) DEREF(splat)[splat_idx + i]
	lum = 0;
#else
				   float eta_vm) {
#endif
	float avg_len = 0;
	uint cnt = 1;
	const float radius = pc.radius;
	const float radius_sqr = radius * radius;

#if VC_MLT == 1
	const uint selected_light_path = min(uint(mlt_rand(mlt_seed, large_step) * screen_size), screen_size - 1);
	uint light_path_idx = selected_light_path;
	uint light_splat_idx = light_path_idx * pc.max_depth * (pc.max_depth + 1);
	uint light_path_len = DEREF(path_cnt)[light_path_idx];
	mlt_sampler.splat_cnt = 0;
	if (save_radiance && DEREF(connected_lights)[selected_light_path] > 0) {
		const uint light_splat_cnt = DEREF(light_splat_cnts)[selected_light_path];
		for (int i = 0; i < light_splat_cnt; i++) {
			const vec3 light_L = DEREF(light_splats)[light_splat_idx + i].L;
			lum += luminance(light_L);
			const uint splat_cnt = mlt_sampler.splat_cnt;
			mlt_sampler.splat_cnt++;
			splat(splat_cnt).idx = DEREF(light_splats)[light_splat_idx + i].idx;
			splat(splat_cnt).L = light_L;
		}
	} else if (DEREF(connected_lights)[selected_light_path] > 0) {
		lum += DEREF(tmp_lum)[selected_light_path];
	}
	light_path_idx *= (pc.max_depth + 1);
#elif VCM_MLT == 1
	const uint num_light_paths = pc.width * pc.height;
	uint light_path_idx = uint(mlt_rand(seed, large_step) * num_light_paths);
	uint light_splat_idx = light_path_idx * pc.max_depth * (pc.max_depth + 1);
	uint light_path_len = DEREF(path_cnt)[light_path_idx];
	mlt_sampler.splat_cnt = 0;
	light_path_idx *= (pc.max_depth + 1);
#else
    uint light_path_idx = uint(rand(seed) * screen_size);
    uint light_path_len = DEREF(path_cnt)[light_path_idx];
    light_path_idx *= (pc.max_depth + 1);
#endif
	vec3 col = vec3(0);
	int depth;
	const float normalization_factor = 1. / (PI * radius_sqr * screen_size);

	for (depth = 1;; depth++) {
		traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, camera_state.pos, tmin, camera_state.wi, tmax, 0);

		if (payload.material_idx == -1) {
			// TODO:
			col += camera_state.throughput * pc.sky_col;
			break;
		}
		vec3 wo = camera_state.pos - payload.pos;
		float dist = length(payload.pos - camera_state.pos);
		float dist_sqr = dist * dist;
		wo /= dist;
		vec3 n_s = payload.n_s;
		vec3 n_g = payload.n_g;
		bool side = true;
		if (dot(payload.n_g, wo) < 0.) n_g = -n_g;
		if (dot(n_g, n_s) < 0) {
			n_s = -n_s;
			side = false;
		}
		const float cos_wo = abs(dot(wo, n_g));

		const Material mat = load_material(payload.material_idx, payload.uv);
		const bool mat_specular = (mat.bsdf_props & BSDF_FLAG_SPECULAR) == BSDF_FLAG_SPECULAR;
		// Complete the missing geometry terms
		camera_state.d_vcm *= dist_sqr;
		camera_state.d_vcm /= cos_wo;
		camera_state.d_vc /= cos_wo;
		camera_state.d_vm /= cos_wo;
		// Get the radiance
		if (luminance(mat.emissive_factor) > 0) {
			col += camera_state.throughput * vcm_get_light_radiance(mat, camera_state, depth);
			// if (pc.use_vc == 1 || pc.use_vm == 1) {
			//     // break;
			// }
		}
		// Connect to light
		if (!mat_specular && depth < pc.max_depth) {
			col += vcm_connect_light(n_s, n_g, wo, mat, side, eta_vm, camera_state);
		}

		// Connect to light vertices
		if (!mat_specular) {
			col += vcm_connect_light_vertices(light_path_len, light_path_idx, depth, n_s, n_g, wo, mat, side, eta_vm,
											  camera_state);
		}
#if VC_MLT == 0
		// Vertex merging
		float r_sqr = radius * radius;
		if (!mat_specular && pc.use_vm == 1) {
			col += vcm_merge_light_vertices(light_path_len, light_path_idx, depth, n_s, n_g, wo, mat, side, eta_vc,
											  camera_state, radius, normalization_factor);
		}
#endif
		if (depth >= pc.max_depth) {
			break;
		}

		// Scattering
		vec3 f;
		float pdf_dir;
		float cos_theta;
#if VC_MLT == 1 || VCM_MLT == 1
		vec3 rands_dir =
			vec3(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
		f = sample_bsdf(n_s, n_g, wo, mat, TRANSPORT_MODE_FROM_CAMERA, side, camera_state.wi, pdf_dir, cos_theta,
						rands_dir);
#else
		f = sample_bsdf(n_s, n_g, wo, mat, TRANSPORT_MODE_FROM_CAMERA, side, camera_state.wi, pdf_dir, cos_theta,
						seed);
#endif

		const bool mat_transmissive = (mat.bsdf_props & BSDF_FLAG_TRANSMISSION) == BSDF_FLAG_TRANSMISSION;
		const bool same_hemisphere = same_hemisphere(camera_state.wi, wo, n_s);
		if (f == vec3(0) || pdf_dir == 0 || (!same_hemisphere && !mat_transmissive)) {
			break;
		}
		float pdf_rev = pdf_dir;
		if (!mat_specular) {
			pdf_rev = bsdf_pdf(mat, n_s, n_g, camera_state.wi, wo, side);
		}
		const float abs_cos_theta = abs(cos_theta);
		const float mis_cos_theta = abs(dot(camera_state.wi, n_g));

		camera_state.pos = offset_ray(payload.pos, n_g, dot(camera_state.wi, n_g) < 0.0);
		// Note, same cancellations also occur here from now on
		// see _vcm_generate_light_sample_
		if (!mat_specular) {
			camera_state.d_vc =
				(mis_cos_theta / pdf_dir) * (eta_vm + camera_state.d_vcm + pdf_rev * camera_state.d_vc);
			camera_state.d_vm =
				(mis_cos_theta / pdf_dir) * (1 + camera_state.d_vcm * eta_vc + pdf_rev * camera_state.d_vm);
			camera_state.d_vcm = 1.0 / pdf_dir;
		} else {
			camera_state.d_vcm = 0;
			camera_state.d_vc *= mis_cos_theta;
			camera_state.d_vm *= mis_cos_theta;
		}

		camera_state.throughput *= f * abs_cos_theta / pdf_dir;
		camera_state.n_s = n_s;
		camera_state.area = payload.area;
		cnt++;
	}

#undef splat
	return col;
}

#if VCM_MLT == 1
float mlt_fill_eye() {
#define cam_vtx(i) DEREF(vcm_vertices)[vcm_light_path_idx + i]
	const float fov = ubo.projection[1][1];
	vec3 cam_pos = vec3(ubo.inv_view * vec4(0, 0, 0, 1));
	vec4 area_int = (ubo.inv_projection * vec4(2. / gl_LaunchSizeEXT.x, 2. / gl_LaunchSizeEXT.y, 0, 1));
	area_int /= area_int.w;
	const float cam_area = abs(area_int.x * area_int.y);
	vec2 dir = vec2(rand(seed), rand(seed)) * 2.0 - 1.0;
	const vec3 direction = sample_camera(dir).xyz;
	VCMState camera_state;
	float lum_sum = 0;
	// Generate camera sample
	camera_state.wi = direction;
	camera_state.pos = cam_pos;
	camera_state.throughput = vec3(1.0);
	camera_state.n_s = vec3(-ubo.inv_view * vec4(0, 0, 1, 0));
	float cos_theta = abs(dot(camera_state.n_s, direction));
	// Defer r^2 / cos term
	camera_state.d_vcm = cam_area * pc.width * pc.height * cos_theta * cos_theta * cos_theta;
	camera_state.d_vc = 0;
	camera_state.d_vm = 0;
	int depth;
	int path_idx = 0;
	ivec2 coords = ivec2(0.5 * (1 + dir) * vec2(pc.width, pc.height));
	uint coords_idx = coords.x * pc.height + coords.y;
	for (depth = 1;; depth++) {
		traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, camera_state.pos, tmin, camera_state.wi, tmax, 0);

		if (payload.material_idx == -1) {
			DEREF(color_storage)[coords_idx] += camera_state.throughput * pc.sky_col;
			break;
		}

		vec3 wo = camera_state.pos - payload.pos;
		float dist = length(payload.pos - camera_state.pos);
		float dist_sqr = dist * dist;
		wo /= dist;
		vec3 n_s = payload.n_s;
		vec3 n_g = payload.n_g;
		bool side = true;
		if (dot(payload.n_g, wo) < 0.) n_g = -n_g;
		if (dot(wo, n_s) < 0.) {
			n_s = -n_s;
			side = false;
		}
		const float cos_wo = abs(dot(wo, n_g));

		const Material mat = load_material(payload.material_idx, payload.uv);
		const bool mat_specular = (mat.bsdf_props & BSDF_FLAG_SPECULAR) == BSDF_FLAG_SPECULAR;
		// Complete the missing geometry terms
		camera_state.d_vcm *= dist_sqr;
		camera_state.d_vcm /= cos_wo;
		camera_state.d_vc /= cos_wo;
		camera_state.d_vm /= cos_wo;

		// Get the radiance
		if (luminance(mat.emissive_factor) > 0) {
			vec3 L = camera_state.throughput * vcm_get_light_radiance(mat, camera_state, depth);
			DEREF(color_storage)[coords_idx] += L;
			lum_sum += luminance(L);
		}

		// Copy to camera vertex buffer
		if (!mat_specular) {
			cam_vtx(path_idx).wo = wo;
			cam_vtx(path_idx).n_s = n_s;
			cam_vtx(path_idx).packed_n_g = pack_normal_octahedral(n_g);
			cam_vtx(path_idx).pos = offset_ray(payload.pos, n_g);
			cam_vtx(path_idx).uv = payload.uv;
			cam_vtx(path_idx).material_idx = payload.material_idx;
			cam_vtx(path_idx).area = payload.area;
			cam_vtx(path_idx).throughput = camera_state.throughput;
			cam_vtx(path_idx).d_vcm = camera_state.d_vcm;
			cam_vtx(path_idx).d_vc = camera_state.d_vc;
			cam_vtx(path_idx).d_vm = camera_state.d_vm;
			cam_vtx(path_idx).path_len = depth + 1;
			cam_vtx(path_idx).side = uint(side);
			cam_vtx(path_idx).coords = coords_idx;
			path_idx++;
		}

		// Connect to light
		if (!mat_specular && depth < pc.max_depth) {
			const vec3 L = vcm_connect_light(n_s, n_g, wo, mat, side, 0, camera_state);
			DEREF(color_storage)[coords_idx] += L;
			lum_sum += luminance(L);
		}

		if (depth >= pc.max_depth) {
			break;
		}
		// Scattering
		vec3 f;
		float pdf_dir;
		float cos_theta;
		f = sample_bsdf(n_s, n_g, wo, mat, TRANSPORT_MODE_FROM_CAMERA, side, camera_state.wi, pdf_dir, cos_theta,
						seed);

		const bool mat_transmissive = (mat.bsdf_props & BSDF_FLAG_TRANSMISSION) == BSDF_FLAG_TRANSMISSION;
		const bool same_hemisphere = same_hemisphere(camera_state.wi, wo, n_s);
		if (f == vec3(0) || pdf_dir == 0 || (!same_hemisphere && !mat_transmissive)) {
			break;
		}
		float pdf_rev = pdf_dir;
		if (!mat_specular) {
			pdf_rev = bsdf_pdf(mat, n_s, n_g, camera_state.wi, wo, side);
		}
		const float abs_cos_theta = abs(cos_theta);
		const float mis_cos_theta = abs(dot(camera_state.wi, n_g));

		camera_state.pos = offset_ray(payload.pos, n_g, dot(camera_state.wi, n_g) < 0.0);
		// Note, same cancellations also occur here from now on
		// see _vcm_generate_light_sample_
		if (!mat_specular) {
			camera_state.d_vc = (mis_cos_theta / pdf_dir) * (camera_state.d_vcm + pdf_rev * camera_state.d_vc);
			camera_state.d_vcm = 1.0 / pdf_dir;
		} else {
			camera_state.d_vcm = 0;
			camera_state.d_vc *= mis_cos_theta;
		}

		camera_state.throughput *= f * abs_cos_theta / pdf_dir;
		camera_state.n_s = n_s;
		camera_state.area = payload.area;
		camera_state.material_idx = payload.material_idx;
	}
	DEREF(path_cnt)[pixel_idx] = path_idx;
#undef cam_vtx
	return lum_sum;
}

float mlt_trace_light() {
#define splat(i) DEREF(splat)[splat_idx + chain * depth_factor + i]
	vec3 cam_pos = vec3(ubo.inv_view * vec4(0, 0, 0, 1));
	vec4 area_int = (ubo.inv_projection * vec4(2. / gl_LaunchSizeEXT.x, 2. / gl_LaunchSizeEXT.y, 0, 1));
	area_int /= area_int.w;
	const float cam_area = abs(area_int.x * area_int.y);
	vec3 cam_nrm = vec3(-ubo.inv_view * vec4(0, 0, 1, 0));
	// Select camera path
	float luminance_sum = 0;
	mlt_sampler.splat_cnt = 0;
	uint path_idx = uint(mlt_rand(mlt_seed, large_step) * (pc.width * pc.height));
	uint path_len = DEREF(path_cnt)[path_idx];
	path_idx *= (pc.max_depth + 1);
	// Trace from light
	VCMState light_state;
	bool finite;
	if (!vcm_generate_light_sample(0, light_state, finite)) {
		return 0;
	}
	int d;
	for (d = 1;; d++) {
		traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, light_state.pos, tmin, light_state.wi, tmax, 0);
		if (payload.material_idx == -1) {
			break;
		}
		const vec3 hit_pos = payload.pos;
		vec3 wo = light_state.pos - hit_pos;

		vec3 n_s = payload.n_s;
		float cos_wo = dot(wo, n_s);
		vec3 n_g = payload.n_g;
		bool side = true;
		if (dot(payload.n_g, wo) <= 0.) n_g = -n_g;
		if (cos_wo <= 0.) {
			cos_wo = -cos_wo;
			n_s = -n_s;
			side = false;
		}

		if (dot(n_g, wo) * dot(n_s, wo) <= 0) {
			// We dont handle BTDF at the moment
			break;
		}
		float dist = length(payload.pos - light_state.pos);
		float dist_sqr = dist * dist;
		wo /= dist;
		const Material mat = load_material(payload.material_idx, payload.uv);
		const bool mat_specular = (mat.bsdf_props & BSDF_FLAG_SPECULAR) == BSDF_FLAG_SPECULAR;
		// Complete the missing geometry terms
		const float cos_theta_wo = abs(dot(wo, n_g));
		// Can't connect from specular to camera path, can't merge either
		if (d > 1 || finite) {
			light_state.d_vcm *= dist_sqr;
		}
		light_state.d_vcm /= cos_theta_wo;
		light_state.d_vc /= cos_theta_wo;
		light_state.d_vm /= cos_theta_wo;
		if (d >= pc.max_depth + 1) {
			break;
		}
		if (d < pc.max_depth) {
			// Connect to camera
			ivec2 coords;
			vec3 splat_col = vcm_connect_cam(cam_pos, cam_nrm, n_s, n_g, cam_area, payload.pos, light_state, 0,
										wo, mat, coords);
			const float lum_val = luminance(splat_col);
			if (lum_val > 0) {
				luminance_sum += lum_val;
				if (save_radiance) {
					const uint idx = coords.x * pc.height + coords.y;
					const uint splat_cnt = mlt_sampler.splat_cnt;
					mlt_sampler.splat_cnt++;
					splat(splat_cnt).idx = idx;
					splat(splat_cnt).L = splat_col;
				}
			}
		}
		vec3 unused;

		if (!mat_specular) {
#define cam_vtx(i) DEREF(vcm_vertices)[i]
			// Connect to cam vertices
			for (int i = 0; i < path_len; i++) {
				uint t = cam_vtx(path_idx + i).path_len;
				uint depth = t + d - 1;
				if (depth >= pc.max_depth) {
					break;
				}
				vec3 dir = hit_pos - cam_vtx(path_idx + i).pos;
				const float len = length(dir);
				const float len_sqr = len * len;
				dir /= len;
				const vec3 cam_n_g = unpack_normal_octahedral(cam_vtx(path_idx + i).packed_n_g);
				const float cos_light = dot(n_g, -dir);
				const float cos_cam = dot(cam_n_g, dir);
				const float G = abs(cos_cam * cos_light) / len_sqr;
				if (G > 0) {
					float pdf_rev = bsdf_pdf(mat, n_s, n_g, -dir, wo, cam_vtx(path_idx + i).side == 1);
					vec3 unused;
					float cam_pdf_fwd, cam_pdf_rev, light_pdf_fwd;
					const Material cam_mat =
						load_material(cam_vtx(path_idx + i).material_idx, cam_vtx(path_idx + i).uv);
					const vec3 f_cam = eval_bsdf(cam_vtx(path_idx + i).n_s, cam_n_g,
												 cam_vtx(path_idx + i).wo, cam_mat, TRANSPORT_MODE_FROM_CAMERA,
										 cam_vtx(path_idx + i).side == 1, dir,
										 cam_pdf_fwd, cam_pdf_rev);
					const vec3 f_light = eval_bsdf(n_s, n_g, wo, mat, TRANSPORT_MODE_FROM_LIGHT, side, -dir,
											   light_pdf_fwd);
					if (f_light != vec3(0) && f_cam != vec3(0)) {
						cam_pdf_fwd *= abs(cos_light) / len_sqr;
						light_pdf_fwd *= abs(cos_cam) / len_sqr;
						const float light_factor = light_state.d_vcm + pdf_rev * light_state.d_vc;
						const float cam_factor =
							cam_vtx(path_idx + i).d_vcm + cam_pdf_rev * cam_vtx(path_idx + i).d_vc;
						const float w_light = light_factor == 0.0 ? 0.0 : cam_pdf_fwd * light_factor;
						const float w_cam = cam_factor == 0.0 ? 0.0 : light_pdf_fwd * cam_factor;
						const float mis_weight = 1. / (1 + w_light + w_cam);
						if (!(mis_weight > 0)) {
							continue;
						}
						const vec3 ray_origin = offset_ray(hit_pos, n_g, dot(-dir, n_g) < 0.0);
						const bool visible = !connection_occluded(ray_origin, -dir, len, 0xFF);
						if (visible) {
							const vec3 L = mis_weight * G * light_state.throughput * cam_vtx(path_idx + i).throughput *
										   f_cam * f_light;
							luminance_sum += luminance(L);
							if (save_radiance) {
								const uint idx = cam_vtx(path_idx + i).coords;
								const uint splat_cnt = mlt_sampler.splat_cnt;
								mlt_sampler.splat_cnt++;
								splat(splat_cnt).idx = idx;
								splat(splat_cnt).L = L;
							}
						}
					}
				}
			}
		}
		// Continue the walk
		float pdf_dir;
		float cos_theta;
		vec3 rands_dir =
			vec3(mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step), mlt_rand(mlt_seed, large_step));
		const vec3 f = sample_bsdf(n_s, n_g, wo, mat, TRANSPORT_MODE_FROM_LIGHT, side, light_state.wi,
								 pdf_dir, cos_theta, rands_dir);
		const bool same_hemisphere = same_hemisphere(light_state.wi, wo, n_s);

		const bool mat_transmissive = (mat.bsdf_props & BSDF_FLAG_TRANSMISSION) == BSDF_FLAG_TRANSMISSION;
		if (f == vec3(0) || pdf_dir == 0 || (!same_hemisphere && !mat_transmissive)) {
			break;
		}

		float pdf_rev = pdf_dir;
		if (!mat_specular) {
			pdf_rev = bsdf_pdf(mat, n_s, n_g, light_state.wi, wo, side);
		}
		const float abs_cos_theta = abs(cos_theta);
		const float mis_cos_theta = abs(dot(light_state.wi, n_g));

		light_state.pos = offset_ray(payload.pos, n_g, dot(light_state.wi, n_g) < 0.0);
		// Note, same cancellations also occur here from now on
		// see _vcm_generate_light_sample_
		if (!mat_specular) {
			light_state.d_vc = (mis_cos_theta / pdf_dir) * (light_state.d_vcm + pdf_rev * light_state.d_vc);
			light_state.d_vcm = 1.0 / pdf_dir;
		} else {
			// Specular pdf has value = inf, so d_vcm = 0;
			light_state.d_vcm = 0;
			// pdf_fwd = pdf_rev = delta -> cancels
			light_state.d_vc *= mis_cos_theta;
		}
		light_state.throughput *= f * abs_cos_theta / pdf_dir;
		light_state.n_s = n_s;
		light_state.area = payload.area;
		light_state.material_idx = payload.material_idx;
	}
	return luminance_sum;
#undef splat
#undef cam_vtx
}
#endif
#endif
