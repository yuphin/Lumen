#pragma once
#include "LumenPCH.h"
#include <mitsuba_parser/tinyparser-mitsuba.h>

struct MitsubaParser {

	enum class M_BSDF {
		DIFFUSE,
		ROUGHCONDUCTOR,
		ROUGHPLASTIC
	};
	struct MitsubaBSDF {
		std::string name;
		std::string type;
		std::string texture;
		glm::vec3 albedo;

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
	
};

