#pragma once

#ifndef RENDER_GPUTYPES_H
#define RENDER_GPUTYPES_H

#include "ptpch.h"

namespace Lucerna {

	// Le struct che esistono solo sulla GPU: le costruisce il RenderScene a partire dalla scena
	// e dagli asset (issue #24, #26). Layout std430, devono combaciare con
	// res/shaders/pt/types.glsl. I materiali e le texture qui sono indici, non id: l'indice del
	// materiale nel buffer, il layer nel texture array.

	struct GpuSphere {
		glm::vec4 Position{ 0.0f }; // (x, y, z, raggio)
		float MaterialIndex = 0;
		glm::vec3 padding{ 0.0f };
	};

	struct GpuQuad {
		glm::vec4 PositionLLC{ 0.0f };
		glm::vec4 U{ 0.0f };
		glm::vec4 V{ 0.0f };
		float MaterialIndex = 0;
		float Width = 1.0f;
		float Height = 1.0f;
		float padding = 0.0f;
	};

	// Il Material della AssetLibrary con i layer al posto degli id delle texture. In std430 un
	// vec4 e' allineato a 16 byte ma alignof(glm::vec4) e' 4: senza il padding esplicito il C++
	// metterebbe RefractionColor a 72 mentre lo shader la legge a 80.
	struct GpuMaterial {
		glm::vec4 Color{ 0.0f };
		float Roughness = 1.0f;
		float SpecularProbability = 1.0f;
		float RefractionRatio = 1.0f;
		float EmissiveStrenght = 0.0f;
		glm::vec4 EmissiveColor{ 1.0f };
		glm::vec4 SpecularColor{ 0.0f };
		float RefractionProbability = 0.0f;
		float RefractionRoughness = 1.0f;
		glm::vec2 _pad0{ 0.0f };
		glm::vec4 RefractionColor{ 1.0f };
		float AlbedoTexture = -1.0f;  // layer nell'array a colori, -1 = nessuna
		float Checker = 0.0f;
		float CheckerScale = 8.0f;
		float NoiseType = 0.0f;
		float NoiseScale = 4.0f;
		float NoiseBlend = 1.0f;
		float NoiseOctaves = 5.0f;
		float NoiseTurbulence = 10.0f;
		glm::vec4 NoiseColorA{ 0.0f };
		glm::vec4 NoiseColorB{ 0.0f };
		float Metalness = 0.0f;
		float SpecularTint = 0.0f;
		float NormalTexture = -1.0f;  // layer nell'array di dati
		float NormalStrength = 1.0f;
		float NormalFlipY = 0.0f;
		float AlphaTexture = -1.0f;   // layer nell'array di dati
		float AlphaCutoff = 0.0f;
		float _pad2 = 0.0f;
	};

	static_assert(sizeof(GpuSphere) == 32 && sizeof(GpuQuad) == 64, "GpuSphere e GpuQuad devono combaciare con lo std430");
	static_assert(sizeof(GpuMaterial) == 192, "GpuMaterial deve essere 192 byte per combaciare con lo std430");
	static_assert(offsetof(GpuMaterial, RefractionColor) == 80, "RefractionColor deve stare a 80 (std430 allinea i vec4 a 16)");
	static_assert(offsetof(GpuMaterial, AlbedoTexture) == 96, "AlbedoTexture deve stare a 96");
	static_assert(offsetof(GpuMaterial, NoiseColorA) == 128, "NoiseColorA deve stare a 128");
	static_assert(offsetof(GpuMaterial, Metalness) == 160, "Metalness deve stare a 160");
	static_assert(offsetof(GpuMaterial, NormalTexture) == 168, "NormalTexture deve stare a 168");
	static_assert(offsetof(GpuMaterial, AlphaCutoff) == 184, "AlphaCutoff deve stare a 184");

	// Posizioni e normali stanno in due buffer distinti: le posizioni sono anche l'ingresso dei
	// BLAS, che non devono vedere nient'altro.
	struct TrianglePosition {
		glm::vec4 A;
		glm::vec4 B;
		glm::vec4 C;
	};

	struct TriangleNormal {
		glm::vec4 NormalA;
		glm::vec4 NormalB;
		glm::vec4 NormalC;
	};

	// "Freddo": letto una sola volta sull'hit finale. La uv sta in .xy, A.z porta il materiale
	// del triangolo (-1 = quello della mesh); i vec4 evitano sorprese di stride con lo std430.
	struct TriangleUV {
		glm::vec4 A{ 0.0f };
		glm::vec4 B{ 0.0f };
		glm::vec4 C{ 0.0f };
	};

	// Un'istanza di mesh come la vede lo shader: in piu' l'inversa (il raggio va nello spazio
	// locale), l'AABB world e i triangoli del suo asset, condivisi con le altre istanze. I
	// triangoli dell'asset sono ordinati prima gli opachi, poi i mascherati: il BLAS ne fa due
	// geometrie (ADR 0008).
	struct MeshInfo {
		glm::mat4 Transform{ 1.0f };
		glm::mat4 InvTransform{ 1.0f };
		glm::vec4 BoundsMin{ 0.0f };  // AABB in world space
		glm::vec4 BoundsMax{ 0.0f };
		glm::vec4 LocalMin{ 0.0f };
		glm::vec4 LocalMax{ 0.0f };
		float FirstTriangle = 0;
		float NumTriangles = 0;
		float MaterialIndex = 0;
		float NumOpaque = 0;
	};

	// Emettitore per la Next Event Estimation (light sampling + MIS), uno per sfera/quad emissivo,
	// Sole, luce punto o spot. Solo vec4/ivec4 -> stride 96, niente padding.
	struct GPULight {
		glm::vec4 Emission{ 0.0f };  // rgb = radianza (EmissiveColor*Strength), a = area
		glm::vec4 P{ 0.0f };         // sfera: centro.xyz + raggio(w) ; quad: LLC.xyz
		glm::vec4 U{ 0.0f };         // quad: vettore-spigolo U (fisico) ; sfera: inutilizzato
		glm::vec4 V{ 0.0f };         // quad: vettore-spigolo V (fisico) ; sfera: inutilizzato
		glm::vec4 Normal{ 0.0f };    // quad: normale.xyz ; sfera: inutilizzato
		glm::ivec4 Meta{ 0 };        // x = tipo (0 sfera, 1 quad, 2 Sole, 3 punto, 4 spot), y = objType, z = objIndex
	};

	static_assert(sizeof(GPULight) == 96, "GPULight deve essere 96 byte per combaciare con lo std430");
	static_assert(sizeof(MeshInfo) == 208, "MeshInfo deve combaciare con lo std430 dello shader");
}

#endif // RENDER_GPUTYPES_H
