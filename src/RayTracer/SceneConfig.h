#pragma once
#include "shaders/commons.h"
#include "Framework/Base/String.h"

struct CameraSettings {
	f32 fov = 90.0f;
	glm::vec3 pos = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	glm::vec3 dir = glm ::vec3(0,0,-1.0f);
};
enum IntegratorType : u32 {
	INTEGRATOR_PATH,
	INTEGRATOR_BDPT,
	INTEGRATOR_SPPM,
	INTEGRATOR_VCM,
	INTEGRATOR_PSSMLT,
	INTEGRATOR_SMLT,
	INTEGRATOR_VCMMLT,
	INTEGRATOR_RESTIR,
	INTEGRATOR_RESTIRGI,
	INTEGRATOR_DDGI,
	INTEGRATOR_RESTIRPT,
	INTEGRATOR_IRCACHE,
	INTEGRATOR_COUNT
};

struct PathConfig {};
struct BDPTConfig {};
struct SPPMConfig {
	f32 base_radius = 0.03f;
};
struct VCMConfig {
	f32 radius_factor = 0.025f;
	bool enable_vm = false;
};
struct PSSMLTConfig {
	f32 mutations_per_pixel = 100.0f;
	u32 num_mlt_threads = 360000;
	u32 num_bootstrap_samples = 360000;
};
struct SMLTConfig {
	f32 mutations_per_pixel = 100.0f;
	u32 num_mlt_threads = 360000;
	u32 num_bootstrap_samples = 360000;
};
struct VCMMLTConfig {
	f32 mutations_per_pixel = 100.0f;
	u32 num_mlt_threads = 360000;
	u32 num_bootstrap_samples = 360000;
	f32 radius_factor = 0.025f;
	bool enable_vm = false;
	bool alternate = true;
	bool light_first = false;
};
struct ReSTIRConfig {};
struct ReSTIRGIConfig {};
struct ReSTIRPTConfig {};
struct DDGIConfig {};
struct IRCacheConfig {};

struct SceneCommon {
	CameraSettings cam_settings = {};
	glm::vec3 sky_col = glm::vec3(0);
	u32 path_length = 6;
	lm::String integrator_name = "Path";
};

struct SceneConfig {
	IntegratorType type = INTEGRATOR_PATH;
	SceneCommon common = {};
	union {
		PathConfig path;
		BDPTConfig bdpt;
		SPPMConfig sppm;
		VCMConfig vcm;
		PSSMLTConfig pssmlt;
		SMLTConfig smlt;
		VCMMLTConfig vcmmlt;
		ReSTIRConfig restir;
		ReSTIRGIConfig restirgi;
		ReSTIRPTConfig restirpt;
		DDGIConfig ddgi;
		IRCacheConfig ircache;
	} settings = {};
};
