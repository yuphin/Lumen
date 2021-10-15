#pragma once
#include "LumenPCH.h"
#include "Framework/VulkanBase.h"
#include "Framework/Shader.h"
#include "Framework/Pipeline.h"
#include "Framework/Camera.h"
#include "Framework/Texture.h"
#include "Framework/Window.h"
#include <glm/glm.hpp>
class Scene {
public:
	Scene(int width, int height, int debug) : width(width), height(height), debug(debug),
		vkb(debug) {};
	VulkanBase vkb;
	VulkanContext& vk_ctx = vkb.ctx;
	int width, height, debug;
	virtual void init(Window*) = 0;
	virtual void update() = 0;
	virtual void cleanup() = 0;
};

