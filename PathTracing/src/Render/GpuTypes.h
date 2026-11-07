#pragma once

#ifndef RENDER_GPUTYPES_H
#define RENDER_GPUTYPES_H

#include "ptpch.h"

namespace Lucerna {

	// Le struct che esistono solo sulla GPU: le costruisce il RenderScene a partire dalla scena
	// (issue #24). Layout std430, devono combaciare con res/shaders/pt/types.glsl.
	// Sfere, quad e materiali non stanno qui: la loro struct di scena e' gia' quella GPU.

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

	// Una Mesh della scena come la vede lo shader: in piu' l'inversa (il raggio va nello spazio
	// locale), l'AABB world e quanti dei suoi triangoli sono opachi. I triangoli della mesh sono
	// ordinati prima gli opachi, poi i mascherati: il BLAS ne fa due geometrie (ADR 0008).
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
