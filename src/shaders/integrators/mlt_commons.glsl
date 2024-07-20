#ifndef MLT_COMMONS
#define MLT_COMMONS
uint mlt_get_next() {
	uint cnt;
	if (mlt_sampler.type == 0) {
		cnt = mlt_sampler.num_light_samples++;
	} else if (mlt_sampler.type == 1) {
		cnt = mlt_sampler.num_cam_samples++;
	} else {
		cnt = mlt_sampler.num_connection_samples++;
	}
	return cnt;
}

uint mlt_get_sample_count() {
	uint cnt;
	if (mlt_sampler.type == 0) {
		cnt = mlt_sampler.num_light_samples;
	} else if (mlt_sampler.type == 1) {
		cnt = mlt_sampler.num_cam_samples;
	} else {
		cnt = mlt_sampler.num_connection_samples;
	}
	return cnt;
}

void mlt_start_iteration() { mlt_sampler.iter++; }

void mlt_select_type(uint type) { mlt_sampler.type = type; }

void mlt_start_chain(uint type) {
	mlt_sampler.type = type;
	if (mlt_sampler.type == 0) {
		mlt_sampler.num_light_samples = 0;
	} else if (mlt_sampler.type == 1) {
		mlt_sampler.num_cam_samples = 0;
	} else {
		mlt_sampler.num_connection_samples = 0;
	}
}
float mutate(float val, inout uvec4 seed) {
	float rnd = rand(seed);
	bool add;
	const float mut_size_high = 1. / 64;
	const float mut_size_low = 1. / 1024.;
	const float log_ratio = log(mut_size_high / mut_size_low);
	if (rnd < 0.5) {
		add = true;
		rnd *= 2.0;
	} else {
		add = false;
		rnd = 2.0 * (rnd - 0.5);
	}
	float dv = mut_size_high * exp(rnd * log_ratio);
	if (add) {
		val += dv;
		if (val > 1) val -= 1;
	} else {
		val -= dv;
		if (val < 0) val += 1;
	}
	return val;
}

float mlt_rand(inout uvec4 seed, bool large_step) {
	if (SEEDING == 1) {
		return rand(seed);
	}

	const uint cnt = mlt_get_next();
	const float sigma = 0.01;
	PrimarySample primary_sample = get_primary_sample(cnt);
     uint mlt_sampler_type = mlt_sampler.type;
	if (primary_sample.last_modified < mlt_sampler.last_large_step) {
		primary_sample.val = rand(seed);
		primary_sample.last_modified = mlt_sampler.last_large_step;
	}
	// Backup current sample
	primary_sample.backup = primary_sample.val;
	primary_sample.last_modified_backup = primary_sample.last_modified;
	if (large_step) {
		primary_sample.val = rand(seed);
	} else {
		uint diff = mlt_sampler.iter - primary_sample.last_modified;
		float nrm_sample = sqrt2 * erf_inv(2 * rand(seed) - 1);
		float eff_sigma = sigma * sqrt(float(diff));
		float before = primary_sample.val;
		primary_sample.val += nrm_sample * eff_sigma;
		primary_sample.val -= floor(primary_sample.val);
	}
	primary_sample.last_modified = mlt_sampler.iter;

	if (mlt_sampler_type == 0) {
		light_primary_samples.d[prim_sample_idxs[mlt_sampler_type] + cnt] = primary_sample;
	} else {
		cam_primary_samples.d[prim_sample_idxs[mlt_sampler_type] + cnt] = primary_sample;
	}
	return primary_sample.val;
}

void mlt_accept(bool large_step) {
	if (large_step) {
		mlt_sampler.last_large_step = mlt_sampler.iter;
	}
}
void mlt_reject() {
	const uint light_sample_cnt = mlt_sampler.num_light_samples;
	mlt_select_type(0);
    uint mlt_sampler_type = mlt_sampler.type;
	for (int i = 0; i < light_sample_cnt; i++) {
		// Restore
		PrimarySample primary_sample = get_primary_sample(i);
		if (primary_sample.last_modified == mlt_sampler.iter) {
			primary_sample.val = primary_sample.backup;
			primary_sample.last_modified = primary_sample.last_modified_backup;

			if (mlt_sampler_type == 0) {
				light_primary_samples.d[prim_sample_idxs[mlt_sampler_type] + i] = primary_sample;
			} else {
				cam_primary_samples.d[prim_sample_idxs[mlt_sampler_type] + i] = primary_sample;
			}
		}
	}
	const uint cam_sample_cnt = mlt_sampler.num_cam_samples;
	mlt_select_type(1);
	for (int i = 0; i < cam_sample_cnt; i++) {
		// Restore
		PrimarySample primary_sample = get_primary_sample(i);
		if (primary_sample.last_modified == mlt_sampler.iter) {
			primary_sample.val = primary_sample.backup;
			primary_sample.last_modified = primary_sample.last_modified_backup;

			if (mlt_sampler_type == 0) {
				light_primary_samples.d[prim_sample_idxs[mlt_sampler_type] + i] = primary_sample;
			} else {
				cam_primary_samples.d[prim_sample_idxs[mlt_sampler_type] + i] = primary_sample;
			}
		}
	}
	const uint connections_cnt = mlt_sampler.num_connection_samples;
	mlt_select_type(2);
	for (int i = 0; i < connections_cnt; i++) {
		// Restore
		PrimarySample primary_sample = get_primary_sample(i);
		if (primary_sample.last_modified == mlt_sampler.iter) {
			primary_sample.val = primary_sample.backup;
			primary_sample.last_modified = primary_sample.last_modified_backup;

			if (mlt_sampler_type == 0) {
				light_primary_samples.d[prim_sample_idxs[mlt_sampler_type] + i] = primary_sample;
			} else {
				cam_primary_samples.d[prim_sample_idxs[mlt_sampler_type] + i] = primary_sample;
			}
		}
	}
	mlt_sampler.iter--;
}

#endif