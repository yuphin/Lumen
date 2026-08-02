#include "../../../surface.glsl"

layout(push_constant) uniform _PushConstantRay { PCReSTIRGI pc; };

uint pixel_idx = (gl_LaunchIDEXT.x * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y);
uvec4 seed =
    init_rng(gl_LaunchIDEXT.xy, gl_LaunchSizeEXT.xy, pc.total_frame_num);

const uint flags = gl_RayFlagsOpaqueEXT;
const float tmin = 0.001;
const float tmax = 10000.0;

void init_s(out ReservoirSample s) {
    s.x_v_surface = invalid_surface_ref();
    s.x_s_surface = invalid_surface_ref();
    s.L_o = vec3(0);
    s.f = vec3(0);
    s.p_q = 0;
    s.bsdf_props = 0;
}

void init_reservoir(out Reservoir r) {
    r.w_sum = 0;
    r.W = 0;
    r.m = 0;
    init_s(r.s);
}

void update_reservoir(inout Reservoir r, const ReservoirSample s, float w_i) {
    r.w_sum += w_i;
    r.m++;
    if (rand(seed) < w_i / r.w_sum) {

        r.s = s;
    }
}

float p_hat(const vec3 f) { return length(f); }


uint offset(const uint pingpong) {
    return pingpong * pc.width * pc.height;
}


bool similar(SurfaceData q, SurfaceData q_n) {
    const float depth_threshold = 0.5;
    const float angle_threshold = 25 * PI / 180;
    if (q.material_idx != q_n.material_idx ||
        dot(q_n.n_s, q.n_s) < cos(angle_threshold)) {
        return false;
    }
    return true;
}
