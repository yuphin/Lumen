#ifndef PT_COMMONS
#define PT_COMMONS
vec3 uniform_sample_light(const Material mat, vec3 pos, const bool side,
                          const vec3 n_s, const vec3 wo,
                          const bool is_specular) {
    vec3 res = vec3(0);
    // Sample light
    vec3 wi;
    float wi_len;
    float pdf_light_w;
    float pdf_light_a;
    LightRecord record;
    float cos_from_light;
    const vec3 Le =
        sample_light_Li(seed, pos, pc_ray.num_lights, pdf_light_w, wi, wi_len,
                        pdf_light_a, cos_from_light, record);
    const vec3 p = offset_ray2(pos, n_s);
    float bsdf_pdf;
    float cos_x = dot(n_s, wi);
    const uint props = is_specular ? BSDF_ALL : BSDF_ALL & ~BSDF_SPECULAR;
    vec3 f = eval_bsdf(n_s, wo, mat, 1, side, wi, bsdf_pdf, cos_x);
    float pdf_light;
    any_hit_payload.hit = 1;
    traceRayEXT(tlas,
                gl_RayFlagsTerminateOnFirstHitEXT |
                    gl_RayFlagsSkipClosestHitShaderEXT,
                0xFF, 1, 0, 1, p, 0, wi, wi_len - EPS, 1);
    const bool visible = any_hit_payload.hit == 0;
    float old;
    if (visible && pdf_light_w > 0) {
        const float mis_weight =
            is_light_delta(record.flags) ? 1 : 1 / (1 + bsdf_pdf / pdf_light_w);
        res += mis_weight * f * abs(cos_x) * Le / pdf_light_w;
      
        old = mis_weight;
        
    }
    if (get_light_type(record.flags) == LIGHT_AREA) {
        // Sample BSDF
        f = sample_bsdf(n_s, wo, mat, 1, side, wi, bsdf_pdf, cos_x, seed);
        if (bsdf_pdf != 0) {
            traceRayEXT(tlas, flags, 0xFF, 0, 0, 0, p, tmin, wi, tmax, 0);
            if (payload.material_idx == record.material_idx &&
                payload.triangle_idx == record.triangle_idx) {
                const float wi_len = length(payload.pos - pos);
                const float g = abs(dot(payload.n_s, -wi)) / (wi_len * wi_len);
                const float mis_weight =
                    1. / (1 + pdf_light_a / (g * bsdf_pdf));
                res += f * mis_weight * abs(cos_x) * Le / bsdf_pdf;
               
            }
        }
    }
    return res;
}
#endif