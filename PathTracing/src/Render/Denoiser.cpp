#include "Render/Denoiser.h"

namespace Lucerna {

	namespace {
		// Deve combaciare con il blocco push_constant di Denoiser.comp.
		struct DenoiserParams {
			int StepSize;
			float CPhi, NPhi, PPhi, APhi;
			int Remodulate;
			float Strength;
			int AutoFade;
		};
	}

	Denoiser::Denoiser() {
		m_Pipeline = std::make_unique<ComputePipeline>("shaders/spv/Denoiser.spv",
			std::vector<VkDescriptorType>(5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE), static_cast<uint32_t>(sizeof(DenoiserParams)));
	}

	Denoiser::~Denoiser() {
		VulkanContext& vk = VulkanContext::Get();
		vk.DestroyImage(m_PingA);
		vk.DestroyImage(m_PingB);
	}

	void Denoiser::Resize(uint32_t width, uint32_t height) {
		if (m_PingA.Width == width && m_PingA.Height == height)
			return;

		VulkanContext& vk = VulkanContext::Get();
		GpuImage oldA = m_PingA, oldB = m_PingB;
		vk.Defer([oldA, oldB]() mutable {
			VulkanContext::Get().DestroyImage(oldA);
			VulkanContext::Get().DestroyImage(oldB);
		});

		const VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		m_PingA = vk.CreateImage(VK_FORMAT_R32G32B32A32_SFLOAT, width, height, usage);
		m_PingB = vk.CreateImage(VK_FORMAT_R32G32B32A32_SFLOAT, width, height, usage);
		vk.TransitionToGeneral(m_PingA);
		vk.TransitionToGeneral(m_PingB);
	}

	const GpuImage& Denoiser::Run(VkCommandBuffer cmd, const GpuImage& irradiance, const GpuImage& beauty, const GpuImage& albedo,
	                              const GpuImage& normalDepth, float strength, bool autoFade, float cPhi, float nPhi, float pPhi, float aPhi) {
		m_Pipeline->Bind(cmd);

		DenoiserParams params{};
		params.CPhi = cPhi;
		params.NPhi = nPhi;
		params.PPhi = pPhi;
		params.APhi = aPhi;
		params.Strength = strength;
		params.AutoFade = autoFade ? 1 : 0;

		// Passo à-trous raddoppiato ad ogni iterazione: kernel 5x5 "bucato" -> supporto che
		// cresce (5,9,17,33,65 px) a costo per-passata costante.
		const int steps[5] = { 1, 2, 4, 8, 16 };

		for (int i = 0; i < 5; i++) {
			// Ping-pong. i=0 legge l'accumulatore di irradianza; le intermedie restano in
			// irradianza; i=4 rimodula + blend. Sequenza degli output: A,B,A,B,A -> finale in PingA.
			const GpuImage& in = (i == 0) ? irradiance : ((i % 2 == 1) ? m_PingA : m_PingB);
			const GpuImage& out = (i % 2 == 0) ? m_PingA : m_PingB;

			m_Pipeline->Image(0, in).Image(1, out).Image(2, albedo).Image(3, normalDepth).Image(4, beauty).Commit(cmd);

			params.StepSize = steps[i];
			params.Remodulate = i == 4 ? 1 : 0;
			m_Pipeline->PushConstants(cmd, &params, sizeof(params));

			vkCmdDispatch(cmd, (irradiance.Width + 7) / 8, (irradiance.Height + 7) / 8, 1);
			ComputeBarrier(cmd); // ogni passata legge l'output della precedente
		}

		return m_PingA; // l'ultima passata (i=4, pari) ha scritto in PingA
	}
}
