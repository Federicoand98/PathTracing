#pragma once

#ifndef RENDERER_H
#define RENDERER_H

#include "ptpch.h"
#include "../Graphics/Texture.h"
#include "../Graphics/FrameBuffer.h"
#include "Primitives.h"
#include "../Camera.h"
#include "PathTracer.h"
#include "PostProcesser.h"

namespace PathTracer {

	class Renderer {
	public:
		Renderer() = default;

		void Initialize(const Camera& camera, const World& world);
		void Render(const Camera& camera, const World& world);
		void OnResize(uint32_t width, uint32_t height);
		void ResetPathTracingCounter(bool sceneReset = false) { m_PTCounter = 1; m_sceneReset = sceneReset; }

		// Picking: si richiede un pixel, e il frame successivo il risultato e' pronto.
		void RequestPick(int x, int y) { m_PickX = x; m_PickY = y; m_PickRequested = true; }
		bool ConsumePickResult(int& objectType, int& objectIndex) {
			if (!m_PickAvailable) return false;
			objectType = m_PickType;
			objectIndex = m_PickIndex;
			m_PickAvailable = false;
			return true;
		}
		std::shared_ptr<Texture> GetRenderedImage() const { return m_RenderedImage; }
		std::shared_ptr<FrameBuffer> GetFrameBuffer() const { return m_FrameBuffer; }
	public:
		bool PathTracing = true;
		bool PostProcessing = true;
		bool EnvironmentMapping = true;
		bool BVHDebug = false;
		float BVHHeatScale = 64.0f;
		float FireflyClamp = 0.0f;   // 0 = off
		float Exposure = 1.0;
		int m_SamplesPerPixel = 1;
		int m_RayDepth = 5;
	private:
		void DrawSceneQuad();
		void UpdateAccumulationCounter();
	private:
		const Camera* m_Camera = nullptr;
		const World* m_World = nullptr;
		std::shared_ptr<PathTracer> m_PathTracer;
		std::shared_ptr<PostProcesser> m_PostProcesser;
		std::shared_ptr<FrameBuffer> m_FrameBuffer;
		std::shared_ptr<Texture> m_RenderedImage;
		unsigned int m_QuadVAO = 0;
		unsigned int m_QuadVBO = 0;
		uint32_t m_Width = 0, m_Height = 0;
		int m_PTCounter = 1;
		bool m_sceneReset = false;
		bool m_PickRequested = false;
		bool m_PickAvailable = false;
		int m_PickX = -1, m_PickY = -1;
		int m_PickType = -1, m_PickIndex = -1;
	};
}

#endif // RENDERER_H