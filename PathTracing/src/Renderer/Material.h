#pragma once

#ifndef MATERIAL_H
#define MATERIAL_H

#include "ptpch.h"
#include "../Random.h"

namespace PathTracer {

	// ATTENZIONE al layout: questo struct viene caricato tale e quale in un SSBO std430.
	// In std430 un vec4 e' allineato a 16 byte, ma alignof(glm::vec4) e' 4: senza il
	// padding esplicito qui sotto il C++ metterebbe RefractionColor a 72 mentre lo shader
	// la legge a 80, e l'intero struct avrebbe stride 96 invece di 112.
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
		glm::vec2 _pad0{ 0.0f };     // porta RefractionColor all'offset 80 (multiplo di 16)
		glm::vec4 RefractionColor{0.0f};

		float AlbedoTexture = -1.0f; // layer nel sampler2DArray, -1 = nessuna texture
		float Checker = 0.0f;        // 0 = off, 1 = scacchiera UV, 2 = scacchiera world-space
		float CheckerScale = 8.0f;   // celle per unita' di UV (o di spazio, se world-space)
		float _pad1 = 0.0f;          // porta la dimensione a 112, multiplo di 16
	};

	static_assert(sizeof(Material) == 112, "Material deve essere 112 byte per combaciare con lo std430");
	static_assert(offsetof(Material, RefractionColor) == 80, "RefractionColor deve stare a 80 (std430 allinea i vec4 a 16)");
	static_assert(offsetof(Material, AlbedoTexture) == 96, "AlbedoTexture deve stare a 96");

	static Material CreateDefaultDiffuse(glm::vec4 color = {0.4f, 0.2, 0.1f, 1.0f}) {
		Material m;
		m.Color = color;
		m.SpecularColor = m.Color;
		m.EmissiveColor = m.Color;
		m.RefractionColor = m.Color;
		m.Roughness = 1.0f;
		return m;
	}

	static Material CreateDefaultMetal(glm::vec4 color = {0.7f, 0.6f, 0.1f, 1.0f}) {
		Material m;
		m.Color = color;
		m.SpecularColor = m.Color;
		m.EmissiveColor = m.Color;
		m.RefractionColor = m.Color;
		m.Roughness = 0.0f;
		return m;
	}

	static Material CreateDefaultGlossy(glm::vec4 color = {0.7f, 0.6f, 0.1f, 1.0f}, float roughness = 0.3, float probability = 0.05) {
		Material m;
		m.Color = color;
		m.SpecularColor = m.Color;
		m.EmissiveColor = m.Color;
		m.RefractionColor = m.Color;
		m.Roughness = roughness;
		m.SpecularProbability = probability;
		return m;
	}

	static Material CreateDefaultDielectric() {
		Material m;
		m.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		m.SpecularColor = m.Color;
		m.EmissiveColor = m.Color;
		//m.RefractionColor = m.Color;
		m.RefractionRatio = 1.02f;
		m.RefractionProbability = 1.0f;
		m.SpecularProbability = 0.0f;
		m.RefractionRoughness = 0.0f;
		return m;
	}

	static Material CreateDefaultLight(glm::vec4 color = {0.88f, 0.83f, 0.3f, 1.0f}, float strength = 1.0) {
		Material m;
		m.Color = color;
		m.SpecularColor = m.Color;
		m.EmissiveColor = m.Color;
		m.RefractionColor = m.Color;
		m.Roughness = 1.0f;
		m.EmissiveStrenght = strength;
		return m;
	}

	static Material CreateDefaultGlass(glm::vec4 color = { 1.0f,1.0f, 1.0f, 1.0f }, float refrIndex = 1.2, float refrProb = 1.0, float refrRough = 0.0) {
		Material m;
		m.Color = color;
		m.Roughness = 0.0f;
		m.SpecularColor = m.Color;
		m.SpecularProbability = 0.0f;
		m.RefractionRatio = refrIndex;
		m.RefractionProbability = refrProb;
		m.RefractionRoughness = refrRough;
		return m;
	}

	static void SetColor(Material& material, const glm::vec4& color) {
		material.Color = color;
		material.EmissiveColor = color;
		material.RefractionColor = color;
		material.SpecularColor = color;
	}

	static Material CreateRandom(bool withRefraction) {
		float rnd = Random::GetFloat(0, 1);
		Material m;

		if (rnd < 0.03) {
			// Light
			m.Color = { 0.88f, 0.83f, 0.3f, 1.0f };
			m.SpecularColor = m.Color;
			m.Roughness = 1.0f;
			m.EmissiveColor = m.Color;
			m.EmissiveStrenght = 1.0f;
		}
		else if (rnd < 0.30) {
			// Metal
			m.Color = glm::vec4(Random::GetVec3(0.5, 1), 1.0f);
			m.SpecularColor = m.Color;
			m.RefractionColor = m.Color;
			m.EmissiveColor = m.Color;
			m.Roughness = 0.0f;
		}
		else if (rnd < 0.5) {
			// Glossy
			m.Color = glm::vec4(Random::GetVec3(0, 1), 1.0f);
			m.Roughness = Random::GetFloat(0.2, 0.35);
			m.SpecularProbability = Random::GetFloat(0.01, 0.07);
			m.SpecularColor = m.Color;
			m.RefractionColor = m.Color;
			m.EmissiveColor = m.Color;
		}
		else if(rnd < 0.65 && withRefraction) {
			// Glass transparent
			m.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
			m.SpecularColor = m.Color;
			m.RefractionColor = m.Color;
			m.SpecularProbability = 0.0f;
			m.RefractionRatio = Random::GetFloat(1.0, 2.5);
			m.RefractionProbability = 1.0f;
			m.RefractionRoughness = 0.0f;
		}
		else if (rnd < 0.68 && withRefraction) {
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
			// Diffuse
			m.Color = glm::vec4(Random::GetVec3(0, 1), 1.0f);
			m.SpecularColor = m.Color;
			m.RefractionColor = m.Color;
			m.EmissiveColor = m.Color;
			m.Roughness = Random::GetFloat(0.5, 1);
		}

		return m;
	}
}

#endif // MATERIAL_H
