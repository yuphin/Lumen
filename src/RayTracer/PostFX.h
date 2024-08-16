#pragma once
#include "LumenPCH.h"
#include "Framework/Texture.h"
#include "Framework/RenderGraph.h"
#include "Framework/LumenInstance.h"
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
	vk::Texture* kernel_ping;
	vk::Texture* kernel_pong;
	vk::Texture* fft_ping_padded;
	vk::Texture* fft_pong_padded;
	VkSampler img_sampler;

	PCPost pc_post_settings;
	bool enable_tonemapping = false;
	bool enable_bloom = false;
	float bloom_exposure = 1e-5f;
	float bloom_amount = 0.26f;
};
