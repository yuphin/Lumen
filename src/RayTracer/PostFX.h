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
	void init(lumen::LumenInstance& instance);
	void render(lumen::Texture2D& input, lumen::Texture2D& output);
	bool gui();
	void destroy();

private:
	lumen::Texture2D kernel_ping;
	lumen::Texture2D kernel_pong;
	lumen::Texture2D fft_ping_padded;
	lumen::Texture2D fft_pong_padded;
	VkSampler img_sampler;

	PCPost pc_post_settings;
	bool enable_tonemapping = false;
	bool enable_bloom = false;
	float bloom_exposure = 1e-5f;
	float bloom_amount = 0.26f;
};
