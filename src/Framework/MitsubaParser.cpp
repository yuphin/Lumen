#include "LumenPCH.h"
#include "MitsubaParser.h"
#include <mitsuba_parser/tinyparser-mitsuba.h>
using namespace TPM_NAMESPACE;

void MitsubaParser::parse(const std::string& path) {
	SceneLoader loader;
	auto scene = loader.loadFromFile(path);

	for (const auto& child : scene.anonymousChildren()) {
		Object* obj = child.get();
		if (obj->type() == OT_BSDF) {
			MitsubaBSDF bsdf;
			bsdf.name = obj->id();
			for (const auto& bsdf_child : obj->anonymousChildren()) {
				Object* p = bsdf_child.get();
				bsdf.type = p->pluginType();
				for (const auto& named_child : p->namedChildren()) {
					if (named_child.second.get()->type() == OT_TEXTURE) {
						for (const auto& texture_prop : named_child.second.get()->properties()) {
							if (texture_prop.first == "filename") {
								bsdf.texture = texture_prop.second.getString();
							}
						}

					}
				}
				for (const auto& prop : p->properties()) {
					if (prop.second.type() == PT_COLOR) {
						// Assume RGB for the moment
						bsdf.albedo = glm::vec3( {
							prop.second.getColor().r,
							prop.second.getColor().g,
							prop.second.getColor().b
						});
					}
				}
			}
			bsdfs.push_back(bsdf);
		} else if (obj->type() == OT_SHAPE) {
			MitsubaMesh mesh;
			for (const auto& prop : obj->properties()) {
				if (prop.first == "filename") {
					mesh.file = prop.second.getString();

				} else if (prop.first == "to_world") {
					float* p_dst = (float*)glm::value_ptr(mesh.transform);
					const auto& src  = prop.second.getTransform();
					for (int i = 0; i < 4; i++) {
						for (int j = 0; j < 4; j++) {
							p_dst[4*i + j] = src.matrix[4*j + i];
						}
					}
				}
			}

			// Assume refs to BSDFs
			for (const auto& mesh_child : obj->anonymousChildren()) {
				auto ref = mesh_child.get()->id();
				mesh.bsdf_ref = ref;
			}

			meshes.push_back(mesh);
		}
	}
}
