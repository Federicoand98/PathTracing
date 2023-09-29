#include "PathTracer.h"

#include <GL/glew.h>

#include "../Camera.h"

namespace PathTracer {

	PathTracer::PathTracer(const class World& world) {
		m_ComputeShader = std::make_shared<ComputeShader>("shaders/PathTracing.comp");
		m_ComputeShader->UpdateWorldBuffer(world);

		m_SkyBox = std::make_shared<Texture>(GL_TEXTURE_CUBE_MAP);
		/*
		m_SkyBox->LoadCubeMap({
				"textures/EnvironmentMap/Temple/posx.png",
				"textures/EnvironmentMap/Temple/negx.png",
				"textures/EnvironmentMap/Temple/posy.png",
				"textures/EnvironmentMap/Temple/negy.png",
				"textures/EnvironmentMap/Temple/posz.png",
				"textures/EnvironmentMap/Temple/negz.png"
			});
		*/

		m_SkyBox->LoadCubeMap({
				"textures/EnvironmentMap/City/posx.jpg",
				"textures/EnvironmentMap/City/negx.jpg",
				"textures/EnvironmentMap/City/posy.jpg",
				"textures/EnvironmentMap/City/negy.jpg",
				"textures/EnvironmentMap/City/posz.jpg",
				"textures/EnvironmentMap/City/negz.jpg"
			});
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
	}

	void PathTracer::UploadUniforms(const ComputeUniformContainer& container) {
		m_ComputeShader->SetInt("SamplerEnvironment", 0);
		m_ComputeShader->SetInt("width", container.Width);
		m_ComputeShader->SetInt("height", container.Height);
		m_ComputeShader->SetInt("rendererFrame", container.NumFrames);
		m_ComputeShader->SetInt("samplesPerPixel", container.SamplesPerPixel);
		m_ComputeShader->SetInt("rayDepth", container.RayDepth);
		m_ComputeShader->SetBool("EnvironmentMapping", container.EnvironmentMapping);
		m_ComputeShader->SetVec3("cameraPosition", container.Camera.GetPosition());
		m_ComputeShader->SetVec3("BackgroundColor", container.World.BackgroundColor);
		m_ComputeShader->SetMat4("inverseProjection", container.Camera.GetInverseProjection());
		m_ComputeShader->SetMat4("inverseView", container.Camera.GetInverseView());
		m_ComputeShader->SetWorld();

		if (container.NumFrames == 1)
			m_ComputeShader->UpdateWorldBuffer(container.World, container.ResetScene);

		m_SkyBox->Bind();
	}
}