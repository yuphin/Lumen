#pragma once
#include "shaders/commons.h"

namespace vk {
struct Texture;
}

struct PostFX {
	vk::Texture* kernel_pong = nullptr;
	vk::Texture* fft_ping_padded = nullptr;
	vk::Texture* fft_pong_padded = nullptr;
	VkSampler img_sampler = VK_NULL_HANDLE;

	PCPost pc_post_settings = {};
	bool enable_tonemapping = false;
	bool enable_bloom = false;
	f32 bloom_exposure = 1e-5f;
	f32 bloom_amount = 0.26f;
};

namespace post_fx {

void init(PostFX* post_fx);
void add_passes(PostFX* post_fx, vk::Texture* input, vk::Texture* output);
bool gui(PostFX* post_fx);
void destroy(PostFX* post_fx);

}  // namespace post_fx
