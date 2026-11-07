#pragma once

#ifndef RENDERER_H
#define RENDERER_H

#include "Render/RenderSettings.h"
#include "ptpch.h"
#include "Scene/Primitives.h"
#include "Render/RenderView.h"
#include "Scene/Scene.h"
#include "Vulkan/ComputePipeline.h"
#include "Render/PathTracer.h"
#include "Render/Denoiser.h"

namespace Lucerna {

	class Renderer {
	public:
		Renderer() = default;

		void Initialize();
		// Consuma i cambiamenti di scena e asset: la scena per la GPU si aggiorna e, se qualcosa che
		// cambia l'immagine e' cambiato, l'accumulo riparte da solo (issue #25).
		void Render(const RenderView& view, Scene& scene, AssetLibrary& assets);
		void OnResize(uint32_t width, uint32_t height);
		// Solo per chi lo chiede esplicitamente (il bottone "Reset Accumulation"): ogni modifica
		// della scena o delle impostazioni che cambia l'immagine fa gia' ripartire l'accumulo.
		void RestartAccumulation() { m_PTCounter = 1; }

		// L'app segnala se la camera si sta muovendo: con RenderScale < 1 questo abbassa
		// la risoluzione di render mentre ci si muove (piu' fluido), tornando a piena
		// risoluzione da fermi. Vedi Render().
		void SetInteracting(bool interacting) { m_Interacting = interacting; }

		// Picking: si richiede un pixel, e il frame successivo il risultato e' pronto.
		void RequestPick(int x, int y) { m_PickX = x; m_PickY = y; m_PickRequested = true; }
		bool ConsumePickResult(int& objectType, int& objectIndex, float& distance) {
			if (!m_PickAvailable) return false;
			objectType = m_PickType;
			objectIndex = m_PickIndex;
			distance = m_PickDistance;
			m_PickAvailable = false;
			return true;
		}
		// L'immagine tonemappata da mostrare. Chi la mostra (l'Editor, con ImGui) la registra da se':
		// la versione cambia quando l'immagine viene ricreata (resize), e va registrata di nuovo.
		VkImageView GetDisplayView() const { return m_Display.View; }
		VkSampler GetDisplaySampler() const { return m_DisplaySampler; }
		uint32_t GetDisplayVersion() const { return m_DisplayVersion; }
		std::string SaveScreenshot(const std::string& path = "");
		void SaveHdr(const std::string& path);
		void Release();
		float GetPathTraceMs() const { return m_PathTraceMs; }
		float GetDenoiseMs() const { return m_DenoiseMs; }
	public:
		RenderSettings Settings; // come si calcola l'immagine: si salva con la scena (ADR 0010)
		ViewSettings View;       // cosa si guarda: mai salvato
	private:
		void UpdateAccumulationCounter();
		void ResizeRenderTargets(uint32_t width, uint32_t height); // colore + AOV, stessa risoluzione
		void ResizeDisplay(uint32_t width, uint32_t height);
		void ReadImage(const GpuImage& image, void* pixels, size_t bytes);
		void ReadTimestamps();
	private:
		RenderView m_View;
		std::unique_ptr<RenderScene> m_Scene;  // la scena per la GPU, aggiornata da scena e asset a ogni frame
		std::unique_ptr<PathTracer> m_PathTracer;
		std::unique_ptr<Denoiser> m_Denoiser;
		std::unique_ptr<ComputePipeline> m_Tonemap;

		GpuImage m_RenderedImage;
		// AOV del frame corrente. Guida del denoiser e della demodulazione.
		GpuImage m_AlbedoAOV;
		// Ping-pong: il compute scrive nell'indice corrente e legge la storia dall'altro.
		// Si scambiano a ogni frame, cosi' la storia e' gratis (nessuna copia).
		// NormalDepth impacchetta normale (rgb) e profondita' (a). Irradiance porta N nell'alpha.
		GpuImage m_NormalDepth[2];
		GpuImage m_Irradiance[2];
		int m_Ping = 0;

		GpuImage m_Display;       // RGBA8 tonemappata, mostrata da ImGui
		VkSampler m_DisplaySampler = VK_NULL_HANDLE;
		uint32_t m_DisplayVersion = 0;
		int screenshotsCount = 0;

		// Timestamp GPU: inizio e fine di path tracer e denoiser. I risultati sono del frame scorso.
		VkQueryPool m_Timestamps = VK_NULL_HANDLE;
		bool m_TimestampsWritten = false;
		bool m_DenoiseWritten = false;
		float m_PathTraceMs = 0.0f;
		float m_DenoiseMs = 0.0f;
		bool m_PickPending = false; // il dispatch del picking e' nel frame scorso, risultato da leggere

		// Frame trascorsi da quando la camera si e' fermata. E' il proxy del minimo N sullo
		// schermo: un pixel disoccluso all'istante dello stop riparte da N=1 e cresce di 1 al
		// frame, quindi dopo k frame fermi il minimo N vale ~k. m_PTCounter non serve piu' allo
		// scopo: con la riproiezione non si azzera piu' al movimento.
		int m_FramesSinceMove = 0;
		// Camera del frame precedente: serve al compute per ritrovare e validare la storia.
		glm::mat4 m_PrevViewProjection{ 1.0f };
		glm::vec3 m_PrevCameraPosition{ 0.0f };
		uint32_t m_Width = 0, m_Height = 0;
		uint32_t m_RenderW = 0, m_RenderH = 0;  // risoluzione effettiva dell'ultimo dispatch
		bool m_Interacting = false;
		int m_PTCounter = 1;
		// Le impostazioni del frame scorso: un cambio di quelle che toccano l'integratore riparte
		RenderSettings m_LastSettings;
		ViewSettings m_LastView;
		bool m_PickRequested = false;
		bool m_PickAvailable = false;
		int m_PickX = -1, m_PickY = -1;
		int m_PickType = -1, m_PickIndex = -1;
		float m_PickDistance = -1.0f;
	};
}

#endif // RENDERER_H