#pragma once

#include "ptpch.h"
#include "RenderingUnit.h"
#include "../Graphics/ComputeShader.h"
#include "../Graphics/Texture.h"
#include "../Graphics/TextureArray.h"

namespace PathTracer {

	struct ComputeUniformContainer {
		uint32_t Width;
		uint32_t Height;
		uint32_t NumFrames;
		uint32_t SamplesPerPixel;
		uint32_t RayDepth;
		bool ResetScene;
		bool EnvironmentMapping;
		bool BVHDebug;
		float BVHHeatScale;
		float FireflyClamp;    // luminanza massima per campione, <= 0 = off
		float ApertureRadius;  // raggio lente thin-lens, <= 0 = pinhole (niente DOF)
		float FocusDistance;   // distanza del piano di fuoco
		int AOVView;           // 0 = beauty, 1 = albedo, 2 = normal, 3 = depth, 4 = storia
		// Riproiezione temporale (ADR 0002): camera del frame precedente + stato della storia.
		glm::mat4 PrevViewProjection;
		glm::vec3 PrevCameraPosition;
		bool AovNeedsUpdate;   // true = camera cambiata/reset: ritraccia gli AOV, altrimenti rileggili
		bool HasHistory;       // false = primo frame / reset scena / resize: storia non valida
		bool CameraMoved;      // true = storia riproiettata -> si applica il cap
		int HistoryCap;
		bool EnableNEE;        // false = azzera numLights lato shader (NEE spenta)
		glm::ivec2 PickPixel;  // (-1,-1) = nessuna richiesta di picking
		const class World& World;
		const class Camera& Camera;
	};

	class PathTracer : public RenderingUnit {
	public:
		PathTracer(const class World& world);
		~PathTracer();

		void Begin() override;
		void End() override;

		void DispatchCompute(unsigned int numGroupX, unsigned int numGroupY);
		void UploadUniforms(const ComputeUniformContainer& container);
		void ReadPickResult(int& objectType, int& objectIndex, float& distance);
	private:
		std::shared_ptr<ComputeShader> m_ComputeShader;
		std::shared_ptr<Texture> m_SkyBox;
		TextureArray m_AlbedoTextures;
		std::vector<std::string> m_LoadedTexturePaths; // per ricaricare solo al cambio scena
	};
}
