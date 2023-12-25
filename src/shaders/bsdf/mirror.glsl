vec3 sample_mirror(vec3 n_s, vec3 wo, out vec3 wi, out float pdf_w, out float cos_theta) {
    wi = reflect(-wo, n_s);
    cos_theta = dot(wi, n_s);
    pdf_w = 1.0;
    return vec3(1.0) / cos_theta;
}

vec3 eval_mirror(out float pdf_w, out float pdf_rev_w) {
    pdf_w = 0.0;
    pdf_rev_w = 0.0;
    return vec3(0);
}

float eval_mirror_pdf() {
    return 0.0;
}