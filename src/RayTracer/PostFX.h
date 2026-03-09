#pragma once
#include "Framework/Texture.h"
#include "Framework/RenderGraph.h"
#include "Framework/ImageUtils.h"
#include "Framework/VkUtils.h"
#include "shaders/commons.h"

class PostFX {
   public:
	void init();
	void render(vk::Texture* input, vk::Texture* output);
	bool gui();
	void destroy();

   private:
	void init_fft();
	vk::Texture* kernel_pong = nullptr;
	vk::Texture* fft_ping_padded = nullptr;
	vk::Texture* fft_pong_padded = nullptr;
	VkSampler img_sampler;

	PCPost pc_post_settings;
	bool enable_tonemapping = false;
	bool enable_bloom = false;
	f32 bloom_exposure = 1e-5f;
	f32 bloom_amount = 0.26f;
};
