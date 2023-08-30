#pragma once

#ifndef Material_h__
#define Material_h__

#include <glm/glm.hpp>

struct Material {
	const char* Name;
	glm::vec3 Color {0.0f};
	float Roughness = 1.0f;
	float SpecularProbability = 1.0f;
	bool Refractive = false;
	float RefractionRatio = 1.0f;
	float EmissiveStrenght = 0.0f;
	glm::vec3 EmissiveColor{ 0.0f };

	glm::vec3 GetEmission() const {
		return EmissiveStrenght * EmissiveColor;
	}
};

// Schlick's approximation
static float reflactance(float cosine, float ratio) {
	auto r0 = (1 - ratio) / (1 + ratio);
	r0 = r0 * r0;
	return r0 + (1 - r0) * pow((1 - cosine), 5);
}

#endif
