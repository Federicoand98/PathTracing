#pragma once

#include "ptpch.h"
#include "Vulkan/ComputePipeline.h"

namespace Lucerna {

	// Denoiser à-trous edge-avoiding (Dammertz et al. 2010) come *view filter* read-only:
	// legge il beauty accumulato + gli AOV del path tracer e ne produce una versione ripulita
	// SENZA toccare il buffer di accumulo (che continua a convergere al ground truth).
	// La logica del filtro vive in res/shaders/Denoiser.comp; qui c'e' solo l'orchestrazione
	// delle 5 passate in ping-pong. Vedi docs/adr/0001-denoiser-read-only-view-filter.md.
	class Denoiser {
	public:
		Denoiser();
		~Denoiser();

		// Alloca/ridimensiona le due immagini di ping-pong alla risoluzione di render.
		void Resize(uint32_t width, uint32_t height);

		// Registra le 5 iterazioni à-trous (stepSize 1,2,4,8,16) alternando le due immagini.
		// L'ingresso e' l'ACCUMULATORE di irradianza del path tracer, gia' demodulato (ADR 0002):
		// il filtro non demodula piu' nulla, rimodula solo all'ultima passata e mescola col beauty.
		// irradiance porta N nell'alpha, usato per l'auto-fade per-pixel.
		// normalDepth: rgb = normale world, a = profondita' (impacchettati, vedi Renderer).
		// strength [0,1] = peso manuale; autoFade lo scala anche per 1/sqrt(N), per-pixel.
		// Ritorna l'immagine finale (spazio colore) da passare al tonemap.
		const GpuImage& Run(VkCommandBuffer cmd, const GpuImage& irradiance, const GpuImage& beauty, const GpuImage& albedo,
		                    const GpuImage& normalDepth, float strength, bool autoFade, float cPhi, float nPhi, float pPhi, float aPhi);
	private:
		std::unique_ptr<ComputePipeline> m_Pipeline;
		GpuImage m_PingA;
		GpuImage m_PingB;
	};
}
