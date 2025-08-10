#pragma once
#include "shaders/commons.h"

#define CAST_CONFIG(ptr, cast) ((cast*)ptr)

struct CameraSettings {
	f32 fov;
	glm::vec3 pos = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	glm::vec3 dir = glm ::vec3(0);
	glm::mat4 cam_matrix = glm::mat4();
};

enum class IntegratorType { Path, BDPT, SPPM, VCM, PSSMLT, SMLT, VCMMLT, ReSTIR, ReSTIRGI, ReSTIRPT, DDGI };

struct SceneConfig {
	i32 path_length = 6;
	glm::vec3 sky_col = glm::vec3(0);
	const std::string integrator_name = "Path";
	IntegratorType integrator_type = IntegratorType::Path;
	CameraSettings cam_settings;

	SceneConfig() = default;
	SceneConfig(const std::string& integrator_name, IntegratorType type)
		: integrator_name(integrator_name), integrator_type(type) {}
};

struct PathConfig : SceneConfig {};

struct BDPTConfig : SceneConfig {
	BDPTConfig() : SceneConfig("BDPT", IntegratorType::BDPT) {}
};

struct SPPMConfig : SceneConfig {
	f32 base_radius = 0.03f;
	SPPMConfig() : SceneConfig("SPPM", IntegratorType::SPPM) {}
};

struct VCMConfig : SceneConfig {
	f32 radius_factor = 0.025f;
	bool enable_vm = false;
	VCMConfig() : SceneConfig("VCM", IntegratorType::VCM) {}
};

struct PSSMLTConfig : SceneConfig {
	f32 mutations_per_pixel = 100.0f;
	i32 num_mlt_threads = 360000;
	i32 num_bootstrap_samples = 360000;
	PSSMLTConfig() : SceneConfig("PSSMLT", IntegratorType::PSSMLT) {}
};

struct SMLTConfig : SceneConfig {
	f32 mutations_per_pixel = 100.0f;
	i32 num_mlt_threads = 360000;
	i32 num_bootstrap_samples = 360000;
	SMLTConfig() : SceneConfig("SMLT", IntegratorType::SMLT) {}
};

struct VCMMLTConfig : SceneConfig {
	f32 mutations_per_pixel = 100.0f;
	i32 num_mlt_threads = 360000;
	i32 num_bootstrap_samples = 360000;
	f32 radius_factor = 0.025f;
	bool enable_vm = false;
	bool alternate = true;
	bool light_first = false;
	VCMMLTConfig() : SceneConfig("VCMMLT", IntegratorType::VCMMLT) {}
};

struct ReSTIRConfig : SceneConfig {
	ReSTIRConfig() : SceneConfig("ReSTIR", IntegratorType::ReSTIR) {}
};

struct ReSTIRGIConfig : SceneConfig {
	ReSTIRGIConfig() : SceneConfig("ReSTIR GI", IntegratorType::ReSTIRGI) {}
};

struct DDGIConfig : SceneConfig {
	DDGIConfig() : SceneConfig("DDGI", IntegratorType::DDGI) {}
};

struct ReSTIRPTConfig : SceneConfig {
	ReSTIRPTConfig() : SceneConfig("ReSTIR PT", IntegratorType::ReSTIRPT) {}
};
