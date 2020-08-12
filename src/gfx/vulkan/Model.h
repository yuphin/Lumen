#pragma once
#include "VKStructs.h"
#include "lmhpch.h"
struct VertexLayout {

	std::vector<vks::Component> components;

	VertexLayout(const std::vector<vks::Component>& components) : components(components) {

	}

	uint32_t stride();
};
