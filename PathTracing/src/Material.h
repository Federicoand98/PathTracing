#pragma once

#ifndef Material_h__
#define Material_h__

#include <glm/glm.hpp>
#include "Random.h"

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

	void CreateDefaultDiffuse() {
		Name = "Default Diffuse";
		Color = { 0.4f, 0.2, 0.1f };
		Roughness = 1.0f;
	}

	void CreateDefaultMetal() {
		Name = "Default Metal";
		Color = { 0.7f, 0.6f, 0.1f };
		Roughness = 0.0f;
	}

	void CreateDefaultDielectric() {
		Name = "Default Dielectric";
		Color = { 1.0f, 1.0f, 1.0f };
		Refractive = true;
		RefractionRatio = 1.5f;
	}

	void CreateDefaultLight() {
		Name = "Default Light";
		Color = { 0.88f, 0.83f, 0.3f };
		Roughness = 1.0f;
		EmissiveColor = Color;
		EmissiveStrenght = 1.0f;
	}

	void CreateRandom(const char* name) {
		float rnd = Random::GetFloat(0, 1);

		Name = name;

		if (rnd < 0.8) {
			// Diffuse
			Color = Random::GetVec3(0, 1);
			Roughness = Random::GetFloat(0.5, 1);
		}
		else if (rnd < 0.95) {
			// Metal
			Color = Random::GetVec3(0.5, 1);
			Roughness = 0.0f;
		}
		else if(rnd < 0.98) {
			// Glass
			Color = { 1.0f, 1.0f, 1.0f };
			Refractive = true;
			RefractionRatio = Random::GetFloat(1.4, 1.8);
		}
		else {
			// Light
			Color = { 0.88f, 0.83f, 0.3f };
			Roughness = 1.0f;
			EmissiveColor = Color;
			EmissiveStrenght = 1.0f;
		}
	}
};

// Schlick's approximation
static float reflactance(float cosine, float ratio) {
	auto r0 = (1 - ratio) / (1 + ratio);
	r0 = r0 * r0;
	return r0 + (1 - r0) * pow((1 - cosine), 5);
}

#endif
