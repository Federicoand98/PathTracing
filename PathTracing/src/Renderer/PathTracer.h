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
		float FireflyClamp;   // luminanza massima per campione, <= 0 = off
		glm::ivec2 PickPixel; // (-1,-1) = nessuna richiesta di picking
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
		void ReadPickResult(int& objectType, int& objectIndex);
	private:
		std::shared_ptr<ComputeShader> m_ComputeShader;
		std::shared_ptr<Texture> m_SkyBox;
		TextureArray m_AlbedoTextures;
		std::vector<std::string> m_LoadedTexturePaths; // per ricaricare solo al cambio scena
	};
}
