#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION 
#include <iostream>
#include <iomanip>
#include <json.hpp>
#include "GltfScene.hpp"
#include <tiny_obj_loader.h>
using json = nlohmann::json;
int main() {
	json j = {
		{"camera",{
			{"position" , {0.7, 0.5, 15.5}},
			{"fov", 45}}
		},
		{"bsdfs", {
				{
					{"type", "glass"},
					{"name", "Glass"},
					{"ior", 1.5},
					{"refs", {"glass"}}
				},
				{
					{"type", "mirror"},
					{"name", "Mirror"},
					{"refs", {"mirror"}}
				},
				{
					{"type", "lambertian"},
					{"name", "Left Wall"},
					{"albedo", {0.63, 0.065, 0.05}},
					{"refs", {"left_wall"}}
				},
				{
					{"type", "lambertian"},
					{"name", "Right Wall"},
					{"albedo", {0.14, 0.45, 0.091}},
					{"refs", {"right_wall"}}
				},
				{
					{"type", "lambertian"},
					{"name", "Other Walls"},
					{"albedo", {0.725,0.71, 0.68}},
					{"refs", {"floor", "ceiling", "back_wall"}}
				},
				{
					{"type", "lambertian"},
					{"name", "Light"},
					{"albedo", {1, 1, 1}},
					{"emissive_factor", {1,1,1}},
					{"refs", {"light"}}
				}
			},
	
		},
	/*	{"lights",
			{{"emissive_factor", {1,1,1}, {"refs", {"light"}}}
			}
		},*/
		{"mesh_file", "specular.obj"}
	};
	std::ofstream o("cornell_box.json");
	o << std::setw(4) << j << std::endl;
	o.close();
	
	const std::string filename = "scenes/specular.obj";

	return 0;
}