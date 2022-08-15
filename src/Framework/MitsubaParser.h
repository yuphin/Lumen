#pragma once
#include "LumenPCH.h"
#include <mitsuba_parser/tinyparser-mitsuba.h>
using namespace TPM_NAMESPACE;

struct MitsubaParser {
	struct MitsubaBSDF {
		std::string name = "";
		std::string type = "";
		std::string texture = "";
		glm::vec3 albedo = glm::vec3(1);
		glm::vec3 emissive_factor = glm::vec3(0);
		float roughness = 0;
		float ior;
	};

	struct MitsubaIntegrator {
		std::string type;
		int depth;
		bool enable_vm = false;
		glm::vec3 sky_col;
	};

	enum class MitsubaShape { Rectangle };

	struct MitsubaLight {
		std::string type;
		glm::vec3 from;
		glm::vec3 to;
		glm::vec3 L;
	};

	struct MitsubaMesh {
		std::string file = "";
		// In case
		std::string bsdf_ref = "";
		int bsdf_idx = -1;
		MitsubaShape shape;
		glm::mat4 transform = glm::mat4(1);
	};

	struct MitsubaCamera {
		float fov;
		glm::mat4 cam_matrix;
	};
	void parse(const std::string& path);

	std::vector<MitsubaBSDF> bsdfs;
	std::vector<MitsubaMesh> meshes;
	std::vector<MitsubaLight> lights;
	MitsubaIntegrator integrator;
	MitsubaCamera camera;
};
