//
// Created by Federico Andrucci on 21/08/23.
//

#include "Renderer.h"

namespace PathTracer {

	void Renderer::OnResize(uint32_t width, uint32_t height) {
		if(m_FrameBuffer) {
			if (m_FrameBuffer->GetWidth() == width && m_FrameBuffer->GetHeight() == height)
				return;

			m_Width = width;
			m_Height = height;
			m_RenderedImage->Resize(width, height);
			m_FrameBuffer->Rescale(width, height);
		} else {
			m_RenderedImage = std::make_shared<Texture>(GL_TEXTURE_2D);
			m_RenderedImage->MutableAllocate(m_Width, m_Height);

			m_FrameBuffer = std::make_shared<FrameBuffer>(width, height);
		}
	}

	void Renderer::Initialize(const Camera& camera, const World& world) {
		m_Camera = &camera;
		m_World = &world;

		m_PathTracer = std::make_shared<PathTracer>(world);
		m_PostProcesser = std::make_shared<PostProcesser>(*m_FrameBuffer);
	}

	void Renderer::Render(const Camera& camera, const World& world) {
		m_RenderedImage->Bind();
		m_RenderedImage->AttachImage(0, 0);

		ComputeUniformContainer container = { m_Width, m_Height, m_PTCounter, m_SamplesPerPixel, m_RayDepth, m_sceneReset, EnvironmentMapping, *m_World, *m_Camera };

		m_PathTracer->Begin();
		m_PathTracer->UploadUniforms(container);
		m_PathTracer->DispatchCompute(m_Width, m_Height);
		m_PathTracer->End();

		glViewport(0, 0, m_Width, m_Height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		m_PostProcesser->Begin();
		m_PostProcesser->UploadUniforms(PostProcessing, Exposure);
		DrawSceneQuad();
		m_PostProcesser->End();

		UpdateAccumulationCounter();
	}

	void Renderer::UpdateAccumulationCounter() {
		if (PathTracing)
			m_PTCounter++;
		else
			m_PTCounter = 1;
		
		if(m_sceneReset)
			m_sceneReset = false;
	}

	void Renderer::DrawSceneQuad() {
		if (m_QuadVAO == 0) {
			float quadVertices[] = {
				// positions        // texture Coords
				-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
				-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
				 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
				 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
			};

			// setup plane VAO
			glGenVertexArrays(1, &m_QuadVAO);
			glGenBuffers(1, &m_QuadVBO);
			glBindVertexArray(m_QuadVAO);
			glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
			glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		}
		glBindVertexArray(m_QuadVAO);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		glBindVertexArray(0);
	}
}