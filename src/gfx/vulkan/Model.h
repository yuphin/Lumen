#pragma once
#include "VKBase.h"

struct VertexLayout {

	std::vector<vks::Component> components;

	VertexLayout(const std::vector<vks::Component>& components) : components(components) {

	}

	uint32_t size() {
		uint32_t res = 0;
		for (auto& component : components) {
			switch (component) {
			case vks::Component::L_UV:
				res += 2 * sizeof(float);
				break;
			default:
				// Rest are 3 floats
				res += 3 * sizeof(float);
			}
		}
		return res;
	}

};
