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
	void init(LumenInstance& instance);
	void render(Texture2D& input, Texture2D& output);
	bool gui();
	void destroy();

private:
	Texture2D kernel_ping;
	Texture2D kernel_pong;
	Texture2D fft_ping_padded;
	Texture2D fft_pong_padded;
	VkSampler img_sampler;

	VulkanContext* ctx = nullptr;
	RenderGraph* rg = nullptr;
	PCPost pc_post_settings;
	bool enable_tonemapping = false;
	bool enable_bloom = true;
	float bloom_exposure = 1e-5f;
	float bloom_amount = 0.26f;
};
