#pragma once

#ifndef MATERIAL_H
#define MATERIAL_H

#include "ptpch.h"
#include "Core/Random.h"
#include "Core/Fields.h"
#include "Core/ObjectId.h"

namespace Lucerna {

	// Un materiale: un asset della AssetLibrary (issue #26). Le texture sono altri asset e si
	// citano per id; il layer del texture array lo sceglie il RenderScene, che ne fa un
	// GpuMaterial (Render/GpuTypes.h) con lo stesso ordine di campi.
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
		// bianco = vetro trasparente (colore trasmesso a distanza unitaria). Nero darebbe
		// vetro completamente assorbente: vedi il Beer-Lambert in PathTracing.comp.
		glm::vec4 RefractionColor{1.0f};

		ObjectId AlbedoTexture;      // texture a colori, nil = nessuna
		float Checker = 0.0f;        // 0 = off, 1 = scacchiera UV, 2 = scacchiera world-space
		float CheckerScale = 8.0f;   // celle per unita' di UV (o di spazio, se world-space)

		// Rumore procedurale valutato in world space (non usa le UV: e' una texture di
		// volume, quindi funziona anche su mesh senza parametrizzazione decente).
		float NoiseType = 0.0f;       // 0 = off, 1 = perlin, 2 = turbolenza, 3 = marmo
		float NoiseScale = 4.0f;      // frequenza: quante celle di rumore per unita' di mondo
		float NoiseBlend = 1.0f;      // 0 = albedo di base, 1 = solo rumore
		float NoiseOctaves = 5.0f;    // ottave della turbolenza (costo lineare!)
		float NoiseTurbulence = 10.0f;// peso della turbolenza nella fase del marmo
		glm::vec4 NoiseColorA{ 0.08f, 0.06f, 0.05f, 1.0f }; // rumore = 0
		glm::vec4 NoiseColorB{ 0.85f, 0.80f, 0.75f, 1.0f }; // rumore = 1

		// 0 = dielettrico (plastica, vetro, intonaco): riflesso non tinto, componente diffusa
		// 1 = metallo: nessuna componente diffusa, il riflesso e' tinto dall'albedo
		float Metalness = 0.0f;
		// 0 = riflesso dielettrico bianco (fisico), 1 = tinto verso la base color (Disney).
		// Ridà i riflessi colorati sui NON metalli senza abbandonare il PBR. I metalli lo
		// ignorano (il loro F0 è già la base color).
		float SpecularTint = 0.0f;

		// Normal map in spazio tangente (ADR 0007): una texture di dati, non sRGB
		ObjectId NormalTexture;      // nil = nessun normal map
		float NormalStrength = 1.0f; // scala la pendenza: 0 = piatto, 1 = come authored
		float NormalFlipY = 0.0f;    // 1 = verde invertito (convenzione DirectX)

		// Alpha mask (ADR 0008). AlphaCutoff > 0 = materiale mascherato: i suoi triangoli vanno
		// nella geometria non opaca del BLAS. Lo decidono i loader, la UI cambia solo la soglia.
		ObjectId AlphaTexture;       // texture di dati (canale rosso), nil = alpha dell'albedo
		float AlphaCutoff = 0.0f;    // il raggio passa dove l'alpha e' sotto la soglia; 0 = opaco
	};

	// Dielettrico opaco "sano" nel senso del modello Principled: riflesso non tinto
	// (SpecularColor bianco), F0 standard 0.04, niente trasmissione. E' il punto di
	// partenza che l'editor Principled si aspetta.
	// Chiavi JSON dei campi (ADR 0010); le texture si scrivono col loro id. I campi assenti in un
	// file restano al default, quindi una scena vecchia resta caricabile quando se ne aggiunge uno.
	constexpr auto JsonFields(const Material*) {
		using M = Material;
		return std::make_tuple(
			Field("color", &M::Color), Field("roughness", &M::Roughness),
			Field("specularProbability", &M::SpecularProbability), Field("refractionRatio", &M::RefractionRatio),
			Field("emissiveStrength", &M::EmissiveStrenght), Field("emissiveColor", &M::EmissiveColor),
			Field("specularColor", &M::SpecularColor), Field("refractionProbability", &M::RefractionProbability),
			Field("refractionRoughness", &M::RefractionRoughness), Field("refractionColor", &M::RefractionColor),
			Field("albedoTexture", &M::AlbedoTexture), Field("checker", &M::Checker), Field("checkerScale", &M::CheckerScale),
			Field("noiseType", &M::NoiseType), Field("noiseScale", &M::NoiseScale), Field("noiseBlend", &M::NoiseBlend),
			Field("noiseOctaves", &M::NoiseOctaves), Field("noiseTurbulence", &M::NoiseTurbulence),
			Field("noiseColorA", &M::NoiseColorA), Field("noiseColorB", &M::NoiseColorB),
			Field("metalness", &M::Metalness), Field("specularTint", &M::SpecularTint),
			Field("normalTexture", &M::NormalTexture), Field("normalStrength", &M::NormalStrength), Field("normalFlipY", &M::NormalFlipY),
			Field("alphaTexture", &M::AlphaTexture), Field("alphaCutoff", &M::AlphaCutoff));
	}

	static Material CreateDefaultPrincipled(glm::vec4 color = { 0.8f, 0.8f, 0.8f, 1.0f }) {
		Material m;
		m.Color = color;
		m.SpecularColor = { 1.0f, 1.0f, 1.0f, 1.0f }; // riflesso dielettrico bianco
		m.RefractionColor = color;                    // tinta del vetro = base color
		m.EmissiveColor = color;
		m.Metalness = 0.0f;
		m.Roughness = 0.5f;
		m.RefractionRoughness = 0.5f;
		m.RefractionRatio = 1.45f;                    // IOR tipico plastica/vetro
		m.SpecularProbability = 0.04f;                // F0 = 0.08 * 0.5
		m.RefractionProbability = 0.0f;               // opaco
		return m;
	}

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
		m.Metalness = 1.0f; // niente diffuso: il colore vive nel riflesso
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
			m.Metalness = 1.0f;
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
