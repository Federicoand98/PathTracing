#pragma once
#include "RenderingUnit.h"
#include <memory>
#include "../Graphics/ComputeShader.h"
#include "../Graphics/Texture.h"

namespace PathTracer {

	struct ComputeUniformContainer {
		uint32_t Width;
		uint32_t Height;
		uint32_t NumFrames;
		uint32_t SamplesPerPixel;
		uint32_t RayDepth;
		bool ResetScene;
		bool EnvironmentMapping;
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
	private:
        unsigned int tbo, trianglesTexture;
		std::shared_ptr<ComputeShader> m_ComputeShader;
		std::shared_ptr<Texture> m_SkyBox;
	};
}
