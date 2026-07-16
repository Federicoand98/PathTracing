//
// Created by Federico Andrucci on 21/08/23.
//

#include "Renderer.h"

#include <algorithm>
#include <cmath>

namespace PathTracer {

	// Il colore e i tre AOV vivono alla stessa risoluzione e vanno ridimensionati insieme:
	// lo shader li scrive con lo stesso texelCoord e imageSize().
	void Renderer::ResizeRenderTargets(uint32_t width, uint32_t height) {
		auto ensure = [&](std::shared_ptr<Texture>& t) {
			if (!t) t = std::make_shared<Texture>(GL_TEXTURE_2D);
			t->Resize(width, height); // Resize gestisce anche la prima allocazione (imposta GL_LINEAR)
		};
		ensure(m_RenderedImage);
		ensure(m_AlbedoAOV);
		ensure(m_NormalAOV);
		ensure(m_DepthAOV);
		// Le ping-pong del denoiser vivono alla stessa risoluzione. Guard: OnResize gira prima
		// di Initialize, quando il denoiser non esiste ancora (Initialize lo ridimensiona poi).
		if (m_Denoiser)
			m_Denoiser->Resize(width, height);
	}

	void Renderer::OnResize(uint32_t width, uint32_t height) {
		if(m_FrameBuffer) {
			if (m_FrameBuffer->GetWidth() == width && m_FrameBuffer->GetHeight() == height)
				return;

			m_Width = width;
			m_Height = height;
			ResizeRenderTargets(width, height);
			m_RenderW = width;   // i target sono ora a piena risoluzione
			m_RenderH = height;
			m_FrameBuffer->Rescale(width, height);
		} else {
			m_Width = width;
			m_Height = height;
			ResizeRenderTargets(width, height);
			m_RenderW = width;
			m_RenderH = height;
			m_FrameBuffer = std::make_shared<FrameBuffer>(width, height);
		}
	}

	void Renderer::Initialize(const Camera& camera, const World& world) {
		m_Camera = &camera;
		m_World = &world;

		m_PathTracer = std::make_shared<PathTracer>(world);
		m_PostProcesser = std::make_shared<PostProcesser>(*m_FrameBuffer);
		m_Denoiser = std::make_shared<Denoiser>();
		m_Denoiser->Resize(m_Width, m_Height); // OnResize ha gia' fissato m_Width/m_Height
	}

	void Renderer::Render(const Camera& camera, const World& world) {
		// Render scale: mentre la camera si muove si renderizza a risoluzione ridotta (meno
		// raggi -> piu' fluido), poi da fermi si torna a piena risoluzione e riparte l'accumulo.
		// L'immagine ridotta viene poi upscalata dal sampler (GL_LINEAR) sull'intero viewport.
		// Il picking forza la piena risoluzione: pickPixel e' in coordinate viewport e su una
		// griglia ridotta non corrisponderebbe a nessun texel.
		bool reduce = m_Interacting && RenderScale < 1.0f && !m_PickRequested;
		uint32_t rw = m_Width, rh = m_Height;
		if (reduce) {
			rw = std::max(8u, static_cast<uint32_t>(m_Width * RenderScale));
			rh = std::max(8u, static_cast<uint32_t>(m_Height * RenderScale));
		}
		if (rw != m_RenderW || rh != m_RenderH) {
			m_RenderW = rw;
			m_RenderH = rh;
			ResizeRenderTargets(rw, rh); // cambio risoluzione: l'accumulo precedente non e' piu' valido
			ResetPathTracingCounter();
		}

		m_RenderedImage->Bind();
		m_RenderedImage->AttachImage(0, 0);
		m_AlbedoAOV->AttachImage(1, 0); // AOV: image unit 1/2/3, scritti dal compute (load+store)
		m_NormalAOV->AttachImage(2, 0);
		m_DepthAOV->AttachImage(3, 0);

		glm::ivec2 pickPixel = m_PickRequested ? glm::ivec2(m_PickX, m_PickY) : glm::ivec2(-1, -1);

		ComputeUniformContainer container = { rw, rh, m_PTCounter, m_SamplesPerPixel, m_RayDepth, m_sceneReset, EnvironmentMapping, BVHDebug, BVHHeatScale, FireflyClamp, Aperture, FocusDistance, AOVView, pickPixel, *m_World, *m_Camera };

		m_PathTracer->Begin();
		m_PathTracer->UploadUniforms(container);
		m_PathTracer->DispatchCompute(rw, rh);

		if (m_PickRequested) {
			m_PathTracer->ReadPickResult(m_PickType, m_PickIndex, m_PickDistance);
			m_PickRequested = false;
			m_PickAvailable = true;
		}

		m_PathTracer->End();

		// Denoiser à-trous (view filter): gira solo sulla vista Beauty, non in debug AOV/BVH.
		// L'output rimpiazza m_RenderedImage come sampler di display (binding point GL_TEXTURE_2D
		// dell'unit 0, distinto dalla cubemap che occupa lo stesso unit). 'strength' e' manuale
		// (slider): un peso costante mantiene visibile e tarabile il filtro. Nota: l'irradianza e'
		// gia' demodulata (liscia), quindi filtrare anche a immagine accumulata non distrugge
		// dettaglio d'albedo, che viene reintrodotto nitido in rimodulazione.
		if (Denoise && AOVView == 0 && !BVHDebug) {
			// Auto-fade: sul frame convergente (N grande) lo strength -> 0, cosi' le ombre soft
			// e l'AO tornano intatte; forte solo quando c'e' rumore da coprire (N piccolo).
			float strength = DenoiseStrength;
			if (DenoiseAutoFade)
				strength *= std::clamp(1.0f / std::sqrt(static_cast<float>(m_PTCounter)), 0.0f, 1.0f);

			auto denoised = m_Denoiser->Run(*m_RenderedImage, *m_AlbedoAOV, *m_NormalAOV, *m_DepthAOV,
			                                strength, DenoiseCPhi, DenoiseNPhi, DenoisePPhi, DenoiseAPhi);
			denoised->Bind(); // TEXTURE0 (2D) = immagine ripulita, campionata dal fragment di display
		}

		glViewport(0, 0, m_Width, m_Height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		m_PostProcesser->Begin();
		m_PostProcesser->UploadUniforms(PostProcessing, Exposure, AOVView);
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