#pragma once
#include "../LumenPCH.h"
#include "Camera.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Texture.h"
#include "VulkanBase.h"
#include "Window.h"
#include <glm/glm.hpp>

namespace lumen {
class LumenInstance {
   public:
	LumenInstance(int width, int height, int debug) : width(width), height(height), debug(debug), vkb(debug){};
	VulkanBase vkb;
	uint32_t width, height, debug;
	virtual void init(Window*) = 0;
	virtual void update() = 0;
	virtual void cleanup() = 0;
	Window* window = nullptr;
};

}  // namespace lumen
