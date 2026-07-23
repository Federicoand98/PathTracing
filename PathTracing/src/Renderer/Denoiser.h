#pragma once

#include "ptpch.h"
#include "../Graphics/ComputeShader.h"
#include "../Graphics/Texture.h"

namespace PathTracer {

	// Denoiser à-trous edge-avoiding (Dammertz et al. 2010) come *view filter* read-only:
	// legge il beauty accumulato + gli AOV del path tracer e ne produce una versione ripulita
	// SENZA toccare il buffer di accumulo (che continua a convergere al ground truth).
	// La logica del filtro vive in res/shaders/Denoiser.comp; qui c'e' solo l'orchestrazione
	// delle 5 passate in ping-pong. Vedi docs/adr/0001-denoiser-read-only-view-filter.md.
	class Denoiser {
	public:
		Denoiser();

		// Alloca/ridimensiona le due texture di ping-pong alla risoluzione di render.
		void Resize(uint32_t width, uint32_t height);

		// Esegue le 5 iterazioni à-trous (stepSize 1,2,4,8,16) alternando le due texture.
		// L'ingresso e' l'ACCUMULATORE di irradianza del path tracer, gia' demodulato (ADR 0002):
		// il filtro non demodula piu' nulla, rimodula solo all'ultima passata e mescola col beauty.
		// irradiance porta N nell'alpha, usato per l'auto-fade per-pixel.
		// normalDepth: rgb = normale world, a = profondita' (impacchettati, vedi Renderer).
		// strength [0,1] = peso manuale; autoFade lo scala anche per 1/sqrt(N), per-pixel.
		// Ritorna la texture finale (spazio colore) da bindare come sampler di display.
		std::shared_ptr<Texture> Run(Texture& irradiance, Texture& beauty, Texture& albedo, Texture& normalDepth,
		                             float strength, bool autoFade, float cPhi, float nPhi, float pPhi, float aPhi);
	private:
		std::shared_ptr<ComputeShader> m_Shader;
		std::shared_ptr<Texture> m_PingA;
		std::shared_ptr<Texture> m_PingB;
		uint32_t m_Width = 0, m_Height = 0;
	};
}
