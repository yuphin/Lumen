#pragma once
#include "LumenPCH.h"
#include "Framework/Camera.h"
#include "Framework/CommandBuffer.h"
#include "Framework/Pipeline.h"
#include "Framework/LumenInstance.h"
#include "Framework/Shader.h"
#include "Framework/Texture.h"
#include "Framework/Utils.h"
#include "Framework/VulkanBase.h"
#include "Framework/Window.h"
#include "Framework/GltfScene.hpp"
#include "Framework/LumenScene.h"
#include "shaders/commons.h"
class Integrator {
public:
	virtual void init() = 0;
	virtual void render() = 0;
	virtual bool update() = 0;
	virtual void destroy() = 0;
	Texture2D output_tex;
	std::unique_ptr<Camera> camera = nullptr;
	bool updated = false;
protected:
	SceneUBO scene_ubo{};
	Buffer vertex_buffer;
	Buffer normal_buffer;
	Buffer uv_buffer;
	Buffer index_buffer;
	Buffer materials_buffer;
	Buffer prim_lookup_buffer;
	Buffer scene_desc_buffer;
	Buffer scene_ubo_buffer;
	Buffer mesh_lights_buffer;
};

