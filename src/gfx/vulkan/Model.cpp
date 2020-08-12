#include "lmhpch.h"
#include "Model.h"
uint32_t VertexLayout::stride() {
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