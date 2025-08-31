#pragma once
#include "shaders/commons.h"
#include "Framework/Base/String.h"

struct CameraSettings {
	f32 fov = 90.0f;
	glm::vec3 pos = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	glm::vec3 dir = glm ::vec3(0,0,-1.0f);
};

// enum class IntegratorType { Path, BDPT, SPPM, VCM, PSSMLT, SMLT, VCMMLT, ReSTIR, ReSTIRGI, ReSTIRPT, DDGI };

// struct SceneConfig {
// 	u32 path_length = 6;
// 	glm::vec3 sky_col = glm::vec3(0);
// 	lm::String integrator_name = "Path";
// 	IntegratorType integrator_type = IntegratorType::Path;
// 	CameraSettings cam_settings;

// 	SceneConfig() = default;
// 	SceneConfig(const lm::String& integrator_name, IntegratorType type)
// 		: integrator_name(integrator_name), integrator_type(type) {}
// };

// struct PathConfig : SceneConfig {};

// struct BDPTConfig : SceneConfig {
// 	BDPTConfig() : SceneConfig("BDPT", IntegratorType::BDPT) {}
// };

// struct SPPMConfig : SceneConfig {
// 	f32 base_radius = 0.03f;
// 	SPPMConfig() : SceneConfig("SPPM", IntegratorType::SPPM) {}
// };

// struct VCMConfig : SceneConfig {
// 	f32 radius_factor = 0.025f;
// 	bool enable_vm = false;
// 	VCMConfig() : SceneConfig("VCM", IntegratorType::VCM) {}
// };

// struct PSSMLTConfig : SceneConfig {
// 	f32 mutations_per_pixel = 100.0f;
// 	u32 num_mlt_threads = 360000;
// 	u32 num_bootstrap_samples = 360000;
// 	PSSMLTConfig() : SceneConfig("PSSMLT", IntegratorType::PSSMLT) {}
// };

// struct SMLTConfig : SceneConfig {
// 	f32 mutations_per_pixel = 100.0f;
// 	u32 num_mlt_threads = 360000;
// 	u32 num_bootstrap_samples = 360000;
// 	SMLTConfig() : SceneConfig("SMLT", IntegratorType::SMLT) {}
// };

// struct VCMMLTConfig : SceneConfig {
// 	f32 mutations_per_pixel = 100.0f;
// 	u32 num_mlt_threads = 360000;
// 	u32 num_bootstrap_samples = 360000;
// 	f32 radius_factor = 0.025f;
// 	bool enable_vm = false;
// 	bool alternate = true;
// 	bool light_first = false;
// 	VCMMLTConfig() : SceneConfig("VCMMLT", IntegratorType::VCMMLT) {}
// };

// struct ReSTIRConfig : SceneConfig {
// 	ReSTIRConfig() : SceneConfig("ReSTIR", IntegratorType::ReSTIR) {}
// };

// struct ReSTIRGIConfig : SceneConfig {
// 	ReSTIRGIConfig() : SceneConfig("ReSTIR GI", IntegratorType::ReSTIRGI) {}
// };

// struct DDGIConfig : SceneConfig {
// 	DDGIConfig() : SceneConfig("DDGI", IntegratorType::DDGI) {}
// };

// struct ReSTIRPTConfig : SceneConfig {
// 	ReSTIRPTConfig() : SceneConfig("ReSTIR PT", IntegratorType::ReSTIRPT) {}
// };

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
	} settings = {};
};
