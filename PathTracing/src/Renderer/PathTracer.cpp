#include "PathTracer.h"

#include <GL/glew.h>

#include "../Camera.h"

namespace PathTracer {

	PathTracer::PathTracer(const class World& world) {
		m_ComputeShader = std::make_shared<ComputeShader>("shaders/PathTracing.comp");
		m_ComputeShader->UpdateWorldBuffer(world);

		// anche senza texture va creato l'array: campionare un sampler incompleto e' UB
		m_AlbedoTextures.Load(world.TexturePaths);
		m_LoadedTexturePaths = world.TexturePaths;

		m_SkyBox = std::make_shared<Texture>(GL_TEXTURE_CUBE_MAP);

		m_SkyBox->LoadCubeMap({
				"textures/EnvironmentMap/Temple/posX.png",
				"textures/EnvironmentMap/Temple/negX.png",
				"textures/EnvironmentMap/Temple/posY.png",
				"textures/EnvironmentMap/Temple/negY.png",
				"textures/EnvironmentMap/Temple/posZ.png",
				"textures/EnvironmentMap/Temple/negZ.png"
			});

		// m_SkyBox->LoadCubeMap({
		// 		"/home/f-andrucci/projects/PathTracing/PathTracing/res/textures/EnvironmentMap/City/posX.jpg",
		// 		"/home/f-andrucci/projects/PathTracing/PathTracing/res/textures/EnvironmentMap/City/negX.jpg",
		// 		"/home/f-andrucci/projects/PathTracing/PathTracing/res/textures/EnvironmentMap/City/posY.jpg",
		// 		"/home/f-andrucci/projects/PathTracing/PathTracing/res/textures/EnvironmentMap/City/negY.jpg",
		// 		"/home/f-andrucci/projects/PathTracing/PathTracing/res/textures/EnvironmentMap/City/posZ.jpg",
		// 		"/home/f-andrucci/projects/PathTracing/PathTracing/res/textures/EnvironmentMap/City/negZ.jpg"
		// 	});
	}

	PathTracer::~PathTracer() {
		m_ComputeShader.reset();
	}

	void PathTracer::Begin() {
		m_ComputeShader->Bind();
	}

	void PathTracer::End() {
		m_ComputeShader->Unbind();
	}

	void PathTracer::DispatchCompute(unsigned int numGroupX, unsigned int numGroupY) {
		glDispatchCompute(numGroupX / 8, numGroupY / 8, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	}

	void PathTracer::ReadPickResult(int& objectType, int& objectIndex) {
		// il dispatch ha appena scritto l'SSBO: serve la barriera prima di rileggerlo
		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
		m_ComputeShader->ReadPickBuffer(objectType, objectIndex);
	}

	void PathTracer::UploadUniforms(const ComputeUniformContainer& container) {
		// il decode delle immagini e' costoso: si ricarica solo se la scena le ha cambiate
		if (container.World.TexturePaths != m_LoadedTexturePaths) {
			m_AlbedoTextures.Load(container.World.TexturePaths);
			m_LoadedTexturePaths = container.World.TexturePaths;
		}

		m_ComputeShader->SetInt("SamplerEnvironment", 0);
		m_ComputeShader->SetInt("AlbedoTextures", 1);
		m_AlbedoTextures.AttachSampler(1); // la cubemap occupa gia' l'unit 0
		m_ComputeShader->SetInt("width", container.Width);
        m_ComputeShader->SetInt("nNodes", container.World.Nodes.size());
		m_ComputeShader->SetInt("height", container.Height);
		m_ComputeShader->SetInt("rendererFrame", container.NumFrames);
		m_ComputeShader->SetInt("samplesPerPixel", container.SamplesPerPixel);
		m_ComputeShader->SetInt("rayDepth", container.RayDepth);
		m_ComputeShader->SetBool("EnvironmentMapping", container.EnvironmentMapping);
		m_ComputeShader->SetInt("bvhDebug", container.BVHDebug ? 1 : 0);
		m_ComputeShader->SetFloat("bvhHeatScale", container.BVHHeatScale);
		m_ComputeShader->SetFloat("fireflyClamp", container.FireflyClamp);
		m_ComputeShader->SetIVec2("pickPixel", container.PickPixel.x, container.PickPixel.y);

		if (container.PickPixel.x >= 0)
			m_ComputeShader->ResetPickBuffer();
		m_ComputeShader->SetVec3("cameraPosition", container.Camera.GetPosition());
		m_ComputeShader->SetVec3("BackgroundColor", container.World.BackgroundColor);
		m_ComputeShader->SetMat4("inverseProjection", container.Camera.GetInverseProjection());
		m_ComputeShader->SetMat4("inverseView", container.Camera.GetInverseView());
		m_ComputeShader->SetWorld();

		if (container.ResetScene || container.NumFrames == 1) {
			m_ComputeShader->UpdateWorldBuffer(container.World);
		}

		m_SkyBox->Bind();
	}
}