#pragma once
#include "shaders/commons.h"

struct CameraSettings {
	float fov;
	glm::vec3 pos = glm::vec3(0);
	glm::vec3 dir = glm ::vec3(0);
	glm::mat4 cam_matrix = glm::mat4();
};

enum class IntegratorType { Path, BDPT, SPPM, VCM, PSSMLT, SMLT, VCMMLT, ReSTIR, ReSTIRGI, DDGI };

struct BaseConfig {
	int path_length = 6;
	glm::vec3 sky_col = glm::vec3(0);
	const std::string integrator_name = "Path";
	IntegratorType integrator_type = IntegratorType::Path;

	BaseConfig() = default;
	BaseConfig(const std::string& integrator_name, IntegratorType type)
		: integrator_name(integrator_name), integrator_type(type) {}
};

struct PathTracerConfig : BaseConfig {};

struct BDPTConfig : BaseConfig {
	BDPTConfig() : BaseConfig("BDPT", IntegratorType::BDPT) {}
};

struct SPPMConfig : BaseConfig {
	float base_radius = 0.03f;
	SPPMConfig() : BaseConfig("SPPM", IntegratorType::SPPM) {}
};

struct VCMConfig : BaseConfig {
	float radius_factor = 0.025f;
	bool enable_vm = false;
	VCMConfig() : BaseConfig("VCM", IntegratorType::VCM) {}
};

struct PSSMLTConfig : BaseConfig {
	float mutations_per_pixel = 100.0f;
	int num_mlt_threads = 360000;
	int num_bootstrap_samples = 360000;
	PSSMLTConfig() : BaseConfig("PSSMLT", IntegratorType::PSSMLT) {}
};

struct SMLTConfig : BaseConfig {
	float mutations_per_pixel = 100.0f;
	int num_mlt_threads = 360000;
	int num_bootstrap_samples = 360000;
	SMLTConfig() : BaseConfig("SMLT", IntegratorType::SMLT) {}
};

struct VCMMLTConfig : BaseConfig {
	float mutations_per_pixel = 100.0f;
	int num_mlt_threads = 360000;
	int num_bootstrap_samples = 360000;
	float base_radius = 0.03f;
	bool enable_vm = false;
	bool alternate = true;
	bool light_first = false;
	VCMMLTConfig() : BaseConfig("VCMMLT", IntegratorType::VCMMLT) {}
};

struct ReSTIRConfig : BaseConfig {
	ReSTIRConfig() : BaseConfig("ReSTIR", IntegratorType::ReSTIR) {}
};

struct ReSTIRGIConfig : BaseConfig {
	ReSTIRGIConfig() : BaseConfig("ReSTIR GI", IntegratorType::ReSTIRGI) {}
};

struct DDGIConfig : BaseConfig {
	DDGIConfig() : BaseConfig("DDGI", IntegratorType::DDGI) {}
};
