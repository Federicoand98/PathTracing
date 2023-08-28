#pragma once

#ifndef Material_h__
#define Material_h__

#include <glm/glm.hpp>

struct Material {
	glm::vec3 Color {0.0f};
	float Roughness = 1.0f;
	float SpecularProbability = 1.0f;
	float EmissiveStrenght = 0.0f;
	glm::vec3 EmissiveColor{ 0.0f };

	glm::vec3 GetEmission() const {
		return EmissiveStrenght * EmissiveColor;
	}
};

#endif
