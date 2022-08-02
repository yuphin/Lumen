#pragma once
#include "LumenPCH.h"
#include "Framework/Camera.h"
#include "Framework/Pipeline.h"
#include "Framework/Shader.h"
#include "Framework/Texture.h"
#include "Framework/VulkanBase.h"
#include "Framework/Window.h"
#include <glm/glm.hpp>
class LumenInstance {
public:
	LumenInstance(int width, int height, int debug)
		: width(width), height(height), debug(debug), vkb(debug) {};
	VulkanBase vkb;
	VulkanContext& vk_ctx = vkb.ctx;
	uint32_t width, height, debug;
	virtual void init(Window*) = 0;
	virtual void update() = 0;
	virtual void cleanup() = 0;
	Window* window = nullptr;
};
