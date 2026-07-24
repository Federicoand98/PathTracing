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
		// ceil: con risoluzioni non multiple di 8 (es. render scale) la divisione intera
		// perderebbe i pixel di bordo. I thread in eccesso escono subito (bounds-guard nello shader).
		glDispatchCompute((numGroupX + 7) / 8, (numGroupY + 7) / 8, 1);
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	}

	void PathTracer::ReadPickResult(int& objectType, int& objectIndex, float& distance) {
		// il dispatch ha appena scritto l'SSBO: serve la barriera prima di rileggerlo
		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
		m_ComputeShader->ReadPickBuffer(objectType, objectIndex, distance);
	}

	void PathTracer::UploadUniforms(const ComputeUniformContainer& container) {
		// il decode delle immagini e' costoso: si ricarica solo se la scena le ha cambiate
		if (container.World.TexturePaths != m_LoadedTexturePaths) {
			m_AlbedoTextures.Load(container.World.TexturePaths);
			m_LoadedTexturePaths = container.World.TexturePaths;
		}

		// L'upload della scena va PRIMA degli uniform: da esso dipende quale variante di shader
		// serve, e ricompilare crea un programma nuovo su cui gli uniform gia' settati sarebbero
		// persi (sono per-programma). Cosi' invece si compila, si binda, e poi si setta tutto.
		if (container.ResetScene || container.NumFrames == 1)
			m_ComputeShader->UpdateWorldBuffer(container.World);

		if (m_ComputeShader->EnsureVariant(m_ComputeShader->PrimBvhRoot() >= 0))
			m_ComputeShader->Bind();

		m_ComputeShader->SetInt("SamplerEnvironment", 0);
		m_ComputeShader->SetInt("AlbedoTextures", 1);
		m_AlbedoTextures.AttachSampler(1); // la cubemap occupa gia' l'unit 0
		m_ComputeShader->SetInt("width", container.Width);
		m_ComputeShader->SetInt("height", container.Height);
		m_ComputeShader->SetInt("rendererFrame", container.NumFrames);
		m_ComputeShader->SetInt("samplesPerPixel", container.SamplesPerPixel);
		m_ComputeShader->SetInt("rayDepth", container.RayDepth);
		m_ComputeShader->SetBool("EnvironmentMapping", container.EnvironmentMapping);
		m_ComputeShader->SetInt("bvhDebug", container.BVHDebug ? 1 : 0);
		m_ComputeShader->SetInt("aovView", container.AOVView);
		m_ComputeShader->SetMat4("prevViewProjection", container.PrevViewProjection);
		m_ComputeShader->SetVec3("prevCameraPosition", container.PrevCameraPosition);
		m_ComputeShader->SetInt("aovNeedsUpdate", container.AovNeedsUpdate ? 1 : 0);
		m_ComputeShader->SetInt("hasHistory", container.HasHistory ? 1 : 0);
		m_ComputeShader->SetInt("cameraMoved", container.CameraMoved ? 1 : 0);
		m_ComputeShader->SetInt("historyCap", container.HistoryCap);
		m_ComputeShader->SetFloat("bvhHeatScale", container.BVHHeatScale);
		m_ComputeShader->SetFloat("fireflyClamp", container.FireflyClamp);
		m_ComputeShader->SetFloat("apertureRadius", container.ApertureRadius);
		m_ComputeShader->SetFloat("focusDistance", container.FocusDistance);
		m_ComputeShader->SetIVec2("pickPixel", container.PickPixel.x, container.PickPixel.y);

		if (container.PickPixel.x >= 0)
			m_ComputeShader->ResetPickBuffer();
		m_ComputeShader->SetVec3("cameraPosition", container.Camera.GetPosition());
		m_ComputeShader->SetVec3("BackgroundColor", container.World.BackgroundColor);
		m_ComputeShader->SetMat4("inverseProjection", container.Camera.GetInverseProjection());
		m_ComputeShader->SetMat4("inverseView", container.Camera.GetInverseView());

		m_ComputeShader->SetInt("numLights", container.EnableNEE ? m_ComputeShader->NumLights() : 0);
		m_ComputeShader->SetInt("primBvhRoot", m_ComputeShader->PrimBvhRoot());
		m_ComputeShader->SetWorld();

		m_SkyBox->Bind();
	}
}