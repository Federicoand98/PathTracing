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
#include "Denoiser.h"

namespace PathTracer {

	class Renderer {
	public:
		Renderer() = default;

		void Initialize(const Camera& camera, World& world);
		void Render(const Camera& camera, const World& world);
		void OnResize(uint32_t width, uint32_t height);
		void ResetPathTracingCounter(bool sceneReset = false) { m_PTCounter = 1; m_sceneReset = sceneReset; }

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
		std::shared_ptr<Texture> GetRenderedImage() const { return m_RenderedImage; }
		std::shared_ptr<FrameBuffer> GetFrameBuffer() const { return m_FrameBuffer; }
	public:
		bool PathTracing = true;
		bool PostProcessing = true;
		bool EnvironmentMapping = true;
		bool BVHDebug = false;
		float BVHHeatScale = 64.0f;
		float FireflyClamp = 0.0f;   // 0 = off
		float Aperture = 0.0f;       // raggio lente; 0 = pinhole, niente DOF
		float FocusDistance = 5.0f;  // distanza del piano di fuoco
		float Exposure = 1.0;
		int AOVView = 0; // 0 = beauty, 1 = albedo, 2 = normal, 3 = depth, 4 = storia
		// Riproiezione temporale (ADR 0002): al posto di azzerare l'accumulo quando la camera
		// si muove, si riusa il frame precedente riproiettandolo. Il cap limita il ghosting da
		// storia stantia e vale solo in movimento; da fermo N cresce libero e converge.
		bool Reproject = true;
		int HistoryCap = 32;
		// Next Event Estimation (light sampling + MIS): a ogni vertice non-speculare campiona
		// direttamente gli emettitori invece di aspettare che un rimbalzo casuale li becchi.
		// Crollo di varianza sulle scene con luci piccole; inattivo (nessun costo) se la scena
		// non ha geometria emissiva. Lato shader e' l'uniform numLights: qui lo si azzera per
		// spegnere la NEE senza toccare la light list.
		bool NEE = true;
		// Denoiser à-trous (view filter read-only): agisce solo sulla vista Beauty, si attenua
		// da solo con l'accumulo. I sigma sono gli edge-stop (luminanza/normale/profondita').
		bool Denoise = false;
		float DenoiseStrength = 1.0f; // [0,1] peso del blend col beauty grezzo (1 = tutto denoised)
		// Auto-attenuazione: scala lo strength ~1/sqrt(N) col numero di campioni. A immagine
		// convergente il denoiser si spegne (niente perdita di ombre soft/AO sul frame pulito);
		// resta forte solo quando serve, cioe' sui frame rumorosi (interattivo, pochi spp).
		bool DenoiseAutoFade = true;
		float DenoiseCPhi = 1.0f;    // sigma luminanza (grande = piu' sfoca ombre/rumore)
		float DenoiseNPhi = 64.0f;   // esponente normale (grande = spigoli piu' netti)
		float DenoisePPhi = 1.0f;    // sigma profondita' in unita' mondo (separa le silhouette)
		float DenoiseAPhi = 0.2f;    // sigma albedo (piccolo = blocca i bordi di materiale/tinta)
		float RenderScale = 1.0f;    // 1 = piena risoluzione; < 1 = ridotta mentre ci si muove
		int m_SamplesPerPixel = 1;
		int m_RayDepth = 5;
	private:
		void DrawSceneQuad();
		void UpdateAccumulationCounter();
		void ResizeRenderTargets(uint32_t width, uint32_t height); // colore + AOV, stessa risoluzione
	private:
		const Camera* m_Camera = nullptr;
		World* m_World = nullptr; // non-const: il Renderer mantiene aggiornato il BVH delle primitive
		std::shared_ptr<PathTracer> m_PathTracer;
		std::shared_ptr<PostProcesser> m_PostProcesser;
		std::shared_ptr<Denoiser> m_Denoiser;
		std::shared_ptr<FrameBuffer> m_FrameBuffer;
		std::shared_ptr<Texture> m_RenderedImage;
		// AOV del frame corrente (image unit 1). Guida del denoiser e della demodulazione.
		std::shared_ptr<Texture> m_AlbedoAOV;
		// Ping-pong: il compute scrive nell'indice corrente e legge la storia dall'altro.
		// Si scambiano a ogni frame, cosi' la storia e' gratis (nessuna copia).
		// NormalDepth impacchetta normale (rgb) e profondita' (a): il driver da' solo 8 image
		// unit e la storia raddoppia il conto dei buffer. Irradiance porta N nell'alpha.
		std::shared_ptr<Texture> m_NormalDepth[2];
		std::shared_ptr<Texture> m_Irradiance[2];
		int m_Ping = 0;
		// Frame trascorsi da quando la camera si e' fermata. E' il proxy del minimo N sullo
		// schermo: un pixel disoccluso all'istante dello stop riparte da N=1 e cresce di 1 al
		// frame, quindi dopo k frame fermi il minimo N vale ~k. m_PTCounter non serve piu' allo
		// scopo: con la riproiezione non si azzera piu' al movimento.
		int m_FramesSinceMove = 0;
		// Camera del frame precedente: serve al compute per ritrovare e validare la storia.
		glm::mat4 m_PrevViewProjection{ 1.0f };
		glm::vec3 m_PrevCameraPosition{ 0.0f };
		unsigned int m_QuadVAO = 0;
		unsigned int m_QuadVBO = 0;
		uint32_t m_Width = 0, m_Height = 0;
		uint32_t m_RenderW = 0, m_RenderH = 0;  // risoluzione effettiva dell'ultimo dispatch
		bool m_Interacting = false;
		int m_PTCounter = 1;
		bool m_sceneReset = false;
		bool m_PickRequested = false;
		bool m_PickAvailable = false;
		int m_PickX = -1, m_PickY = -1;
		int m_PickType = -1, m_PickIndex = -1;
		float m_PickDistance = -1.0f;
	};
}

#endif // RENDERER_H