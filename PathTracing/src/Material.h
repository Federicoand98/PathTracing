#pragma once

#ifndef MATERIAL_H
#define MATERIAL_H

#include <glm/glm.hpp>
#include "Random.h"

struct Material {
	glm::vec4 Color {0.0f};
	float Roughness = 1.0f;
	float SpecularProbability = 1.0f;
	float RefractionRatio = 1.0f;
	float EmissiveStrenght = 0.0f;
	glm::vec4 EmissiveColor{ 0.0f };
};

static Material CreateDefaultDiffuse() {
	Material m;
	m.Color = { 0.4f, 0.2, 0.1f, 1.0f };
	m.Roughness = 1.0f;
	return m;
}

static Material CreateDefaultMetal() {
	Material m;
	m.Color = { 0.7f, 0.6f, 0.1f, 1.0f };
	m.Roughness = 0.0f;
	return m;
}

static Material CreateDefaultDielectric() {
	Material m;
	m.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
	m.RefractionRatio = 1.5f;
	return m;
}

static Material CreateDefaultLight() {
	Material m;
	m.Color = { 0.88f, 0.83f, 0.3f, 1.0f };
	m.Roughness = 1.0f;
	m.EmissiveColor = m.Color;
	m.EmissiveStrenght = 1.0f;
	return m;
}

static Material CreateRandom(const char* name) {
	float rnd = Random::GetFloat(0, 1);
	Material m;

	if (rnd < 0.8) {
		// Diffuse
		m.Color = glm::vec4(Random::GetVec3(0, 1), 1.0f);
		m.Roughness = Random::GetFloat(0.5, 1);
	}
	else if (rnd < 0.95) {
		// Metal
		m.Color = glm::vec4(Random::GetVec3(0.5, 1), 1.0f);
		m.Roughness = 0.0f;
	}
	else if(rnd < 0.98) {
		// Glass
		m.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		m.RefractionRatio = Random::GetFloat(1.4, 1.8);
	}
	else {
		// Light
		m.Color = { 0.88f, 0.83f, 0.3f, 1.0f };
		m.Roughness = 1.0f;
		m.EmissiveColor = m.Color;
		m.EmissiveStrenght = 1.0f;
	}

	return m;
}

// Schlick's approximation
static float reflactance(float cosine, float ratio) {
	auto r0 = (1 - ratio) / (1 + ratio);
	r0 = r0 * r0;
	return r0 + (1 - r0) * pow((1 - cosine), 5);
}

#endif // MATERIAL_H
