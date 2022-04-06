#pragma once
#include "LumenPCH.h"
#include <mitsuba_parser/tinyparser-mitsuba.h>

struct MitsubaParser {

	struct MitsubaBSDF {
		std::string name = "";
		std::string type = "";
		std::string texture = "";
		glm::vec3 albedo = glm::vec3(1);
		float roughness = 0;

	};

	struct MitsubaLight {
		std::string type;
		glm::vec3 from;
		glm::vec3 to;
		glm::vec3 L;

	};

	struct MitsubaMesh {
		std::string file;
		// In case
		std::string bsdf_ref;
		int bsdf_idx;
		glm::mat4 transform;
	};
	void parse(const std::string& path);

	std::vector<MitsubaBSDF> bsdfs;
	std::vector<MitsubaMesh> meshes;
	std::vector<MitsubaLight> lights;
	
};

