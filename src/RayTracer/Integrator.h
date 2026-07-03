#pragma once
#include "Framework/Base/Random.h"
#include "Framework/CommandBuffer.h"
#include "Framework/VulkanBase.h"
#include "Framework/Window.h"
#include "Framework/DynamicResourceManager.h"
#include "Framework/PersistentResourceManager.h"
#include "shaders/commons.h"
#include "LumenScene.h"
#include "Path.h"
#include "BDPT.h"
#include "SPPM.h"
#include "VCM.h"
#include "PSSMLT.h"
#include "SMLT.h"
#include "VCMMLT.h"
#include "ReSTIR.h"
#include "ReSTIRGI.h"
#include "ReSTIRPT.h"
#include "DDGI.h"
#include "IrradianceCache.h"

struct Integrator {
	IntegratorType type = INTEGRATOR_PATH;

	void (*init)(Integrator*) = nullptr;
	void (*render)(Integrator*) = nullptr;
	bool (*update)(Integrator*) = nullptr;
	bool (*gui)(Integrator*) = nullptr;
	void (*destroy)(Integrator*, bool resize) = nullptr;
	void (*create_accel)(Integrator*, vk::BVH*, lm::Array<vk::BVH>*) = nullptr;

	vk::Texture* output_tex = nullptr;
	bool updated = false;
	bool callbacks_registered = false;
	bool initialized = false;
	u32 frame_num = 0;
	SceneUBO scene_ubo{};
	lm::SmallArray<vk::Buffer*, vk::MAX_FRAMES_IN_FLIGHT> scene_ubo_buffers;
	vk::Buffer* scene_ubo_buffer = nullptr;
	vk::BVH* tlas = nullptr;
	scene::Scene* lumen_scene = nullptr;
	lm::Arena* arena = nullptr;

	Path path;
	BDPT bdpt;
	SPPM sppm;
	VCM vcm;
	PSSMLT pssmlt;
	SMLT smlt;
	VCMMLT vcmmlt;
	ReSTIR restir;
	ReSTIRGI restirgi;
	ReSTIRPT restirpt;
	DDGI ddgi;
	IrradianceCache ircache;
};

namespace integrator {

void set_type(Integrator* integrator, IntegratorType type);
void init(Integrator* integrator);
void render(Integrator* integrator);
bool update(Integrator* integrator);
bool gui(Integrator* integrator);
void destroy(Integrator* integrator, bool resize);
void create_accel(Integrator* integrator, vk::BVH* tlas, lm::Array<vk::BVH>* blases);

}  // namespace integrator
