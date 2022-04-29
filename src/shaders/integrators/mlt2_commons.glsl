#ifndef MLT2_COMMONS
#define MLT2_COMMONS
uint mlt_get_next() { return mlt_sampler.num_light_samples++; }

uint mlt_get_sample_count() { return mlt_sampler.num_light_samples; }

void mlt_start_iteration() { mlt_sampler.iter++; }

void mlt_start_chain() { mlt_sampler.num_light_samples = 0; }

float mlt_rand(inout uvec4 seed, bool large_step) {
    if (SEEDING == 1) {
        return rand(seed);
    }
    const uint cnt = mlt_get_next();
    const float sigma = 0.01;
    if (primary_sample(cnt).last_modified < mlt_sampler.last_large_step) {
        primary_sample(cnt).val = rand(seed);
        primary_sample(cnt).last_modified = mlt_sampler.last_large_step;
    }
    // Backup current sample
    primary_sample(cnt).backup = primary_sample(cnt).val;
    primary_sample(cnt).last_modified_backup =
        primary_sample(cnt).last_modified;
    if (large_step) {
        primary_sample(cnt).val = rand(seed);
    } else {
        uint diff = mlt_sampler.iter - primary_sample(cnt).last_modified;
        float nrm_sample = sqrt2 * erf_inv(2 * rand(seed) - 1);
        float eff_sigma = sigma * sqrt(float(diff));
        primary_sample(cnt).val += nrm_sample * eff_sigma;
        primary_sample(cnt).val -= floor(primary_sample(cnt).val);
    }
    primary_sample(cnt).last_modified = mlt_sampler.iter;
    return primary_sample(cnt).val;
}

void mlt_accept(bool large_step) {

    if (large_step) {
        mlt_sampler.last_large_step = mlt_sampler.iter;
    }
}
void mlt_reject() {
    const uint cnt = mlt_get_sample_count();
    for (int i = 0; i < cnt; i++) {
        // Restore
        if (primary_sample(i).last_modified == mlt_sampler.iter) {
            primary_sample(i).val = primary_sample(i).backup;
            primary_sample(i).last_modified =
                primary_sample(i).last_modified_backup;
        }
    }
    mlt_sampler.iter--;
}
#endif