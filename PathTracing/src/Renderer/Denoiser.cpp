#include "Denoiser.h"

#include <GL/glew.h>

namespace PathTracer {

	Denoiser::Denoiser() {
		m_Shader = std::make_shared<ComputeShader>("shaders/Denoiser.comp");
	}

	void Denoiser::Resize(uint32_t width, uint32_t height) {
		if (!m_PingA) m_PingA = std::make_shared<Texture>(GL_TEXTURE_2D);
		if (!m_PingB) m_PingB = std::make_shared<Texture>(GL_TEXTURE_2D);
		m_PingA->Resize(width, height); // no-op se la dimensione non e' cambiata
		m_PingB->Resize(width, height);
		m_Width = width;
		m_Height = height;
	}

	std::shared_ptr<Texture> Denoiser::Run(Texture& beauty, Texture& albedo, Texture& normal, Texture& depth,
	                                       float strength, float cPhi, float nPhi, float pPhi, float aPhi) {
		m_Shader->Bind();
		m_Shader->SetFloat("cPhi", cPhi);
		m_Shader->SetFloat("nPhi", nPhi);
		m_Shader->SetFloat("pPhi", pPhi);
		m_Shader->SetFloat("aPhi", aPhi);
		m_Shader->SetFloat("strength", strength);

		// Passo à-trous raddoppiato ad ogni iterazione: kernel 5x5 "bucato" -> supporto che
		// cresce (5,9,17,33,65 px) a costo per-passata costante.
		const int steps[5] = { 1, 2, 4, 8, 16 };

		for (int i = 0; i < 5; i++) {
			// Ping-pong. i=0 legge il beauty e demodula; le intermedie stanno in irradianza;
			// i=4 rimodula + blend. Sequenza degli output: A,B,A,B,A -> finale in PingA.
			Texture* in  = (i == 0) ? &beauty : ((i % 2 == 1) ? m_PingA.get() : m_PingB.get());
			Texture* out = (i % 2 == 0) ? m_PingA.get() : m_PingB.get();

			in->AttachImage(0, 0);
			out->AttachImage(1, 0);
			albedo.AttachImage(2, 0);
			normal.AttachImage(3, 0);
			depth.AttachImage(4, 0);
			beauty.AttachImage(5, 0); // serve solo a i=4 per il blend, ma bindarlo sempre e' innocuo

			m_Shader->SetInt("stepSize", steps[i]);
			m_Shader->SetInt("demodulate", i == 0 ? 1 : 0);
			m_Shader->SetInt("remodulate", i == 4 ? 1 : 0);

			glDispatchCompute((m_Width + 7) / 8, (m_Height + 7) / 8, 1);
			// ogni passata legge l'output della precedente: barriera image obbligatoria
			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}

		m_Shader->Unbind();
		return m_PingA; // l'ultima passata (i=4, pari) ha scritto in PingA
	}
}
