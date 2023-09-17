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
	glm::vec4 EmissiveColor{ 1.0f };
	glm::vec4 SpecularColor{0.0f};
	float RefractionProbability = 0.0f;
	float RefractionRoughness = 1.0f;
	glm::vec4 RefractionColor{0.0f};

	glm::vec2 padding{0.0f};
};

static Material CreateDefaultDiffuse() {
	Material m;
	m.Color = { 0.4f, 0.2, 0.1f, 1.0f };
	m.SpecularColor = m.Color;
	m.Roughness = 1.0f;
	return m;
}

static Material CreateDefaultMetal() {
	Material m;
	m.Color = { 0.7f, 0.6f, 0.1f, 1.0f };
	m.SpecularColor = m.Color;
	m.Roughness = 0.0f;
	return m;
}

static Material CreateDefaultDielectric() {
	Material m;
	m.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
	m.SpecularColor = m.Color;
	m.RefractionRatio = 1.02f;
	m.RefractionProbability = 1.0f;
	m.SpecularProbability = 0.0f;
	m.RefractionRoughness = 0.0f;
	return m;
}

static Material CreateDefaultLight() {
	Material m;
	m.Color = { 0.88f, 0.83f, 0.3f, 1.0f };
	m.SpecularColor = m.Color;
	m.Roughness = 1.0f;
	m.EmissiveColor = m.Color;
	m.EmissiveStrenght = 1.0f;
	return m;
}

static Material CreateRandom(const char* name) {
	float rnd = Random::GetFloat(0, 1);
	Material m;

	if (rnd < 0.4) {
		// Diffuse
		m.Color = glm::vec4(Random::GetVec3(0, 1), 1.0f);
		m.SpecularColor = m.Color;
		m.Roughness = Random::GetFloat(0.5, 1);
	}
	else if (rnd < 0.55) {
		// Metal
		m.Color = glm::vec4(Random::GetVec3(0.5, 1), 1.0f);
		m.SpecularColor = m.Color;
		m.Roughness = 0.0f;
	}
	else if (rnd < 0.75) {
		// Glossy
		m.Color = glm::vec4(Random::GetVec3(0, 1), 1.0f);
		m.Roughness = Random::GetFloat(0.2, 0.35);
		m.SpecularProbability = Random::GetFloat(0.01, 0.07);
		m.SpecularColor = m.Color;
	}
	else if(rnd < 0.9) {
		// Glass transparent
		m.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		m.SpecularColor = m.Color;
		m.RefractionColor = m.Color;
		m.SpecularProbability = 0.0f;
		m.RefractionRatio = Random::GetFloat(1.0, 2.5);
		m.RefractionProbability = 1.0f;
		m.RefractionRoughness = 0.0f;
	}
	else if (rnd < 0.95) {
		// glass color
		m.Color = glm::vec4(Random::GetVec3(0, 1), 1.0f);
		m.SpecularColor = m.Color;
		m.RefractionColor = m.Color;
		m.SpecularProbability = Random::GetFloat(0.1, 0.4);
		m.RefractionRatio = 1.01;
		m.RefractionProbability = Random::GetFloat(0.7, 1.0);
		m.RefractionRoughness = Random::GetFloat(0, 0.15);
	}
	else {
		// Light
		m.Color = { 0.88f, 0.83f, 0.3f, 1.0f };
		m.SpecularColor = m.Color;
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
