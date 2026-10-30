//
// Created by Federico Andrucci on 21/08/23.
//

#include "Renderer.h"

#include "imgui_impl_vulkan.h"
#include "../stb_image_write.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace PathTracer {

	namespace {
		// Deve combaciare con il blocco push_constant di Tonemap.comp.
		struct TonemapParams {
			float Exposure;
			int PostProcessing;
			int AovView;
		};

		constexpr VkImageUsageFlags TARGET_USAGE = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}

	// Il colore e gli AOV vivono alla stessa risoluzione e vanno ridimensionati insieme:
	// lo shader li scrive con lo stesso texelCoord e imageSize().
	void Renderer::ResizeRenderTargets(uint32_t width, uint32_t height) {
		VulkanContext& vk = VulkanContext::Get();

		auto replace = [&](GpuImage& image) {
			GpuImage old = image;
			vk.Defer([old]() mutable { VulkanContext::Get().DestroyImage(old); });
			image = vk.CreateImage(VK_FORMAT_R32G32B32A32_SFLOAT, width, height, TARGET_USAGE);
			vk.TransitionToGeneral(image);
		};
		replace(m_RenderedImage);
		replace(m_AlbedoAOV);
		for (int i = 0; i < 2; i++) {
			replace(m_NormalDepth[i]);
			replace(m_Irradiance[i]);
		}

		// Guard: OnResize gira prima di Initialize, quando il denoiser non esiste ancora.
		if (m_Denoiser)
			m_Denoiser->Resize(width, height);
	}

	void Renderer::ResizeDisplay(uint32_t width, uint32_t height) {
		VulkanContext& vk = VulkanContext::Get();

		// Le draw list di ImGui di questo frame riferiscono ancora l'immagine vecchia.
		GpuImage old = m_Display;
		VkDescriptorSet oldSet = m_DisplaySet;
		vk.Defer([old, oldSet]() mutable {
			if (oldSet != VK_NULL_HANDLE) ImGui_ImplVulkan_RemoveTexture(oldSet);
			VulkanContext::Get().DestroyImage(old);
		});

		m_Display = vk.CreateImage(VK_FORMAT_R8G8B8A8_UNORM, width, height, TARGET_USAGE);
		vk.TransitionToGeneral(m_Display);
		m_DisplaySet = ImGui_ImplVulkan_AddTexture(m_DisplaySampler, m_Display.View, VK_IMAGE_LAYOUT_GENERAL);
	}

	void Renderer::OnResize(uint32_t width, uint32_t height) {
		VulkanContext& vk = VulkanContext::Get();

		if (m_DisplaySampler == VK_NULL_HANDLE) {
			VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			vkCreateSampler(vk.Device(), &samplerInfo, nullptr, &m_DisplaySampler);
		}

		if ((m_Width == width && m_Height == height) || width == 0 || height == 0)
			return;

		m_Width = width;
		m_Height = height;
		ResizeDisplay(width, height);
		ResizeRenderTargets(width, height);
		m_RenderW = width;   // i target sono ora a piena risoluzione
		m_RenderH = height;
	}

	void Renderer::Initialize(const Camera& camera, World& world) {
		VulkanContext& vk = VulkanContext::Get();
		m_Camera = &camera;
		m_World = &world;

		m_PathTracer = std::make_unique<PathTracer>(world);
		m_Denoiser = std::make_unique<Denoiser>();
		m_Denoiser->Resize(m_Width, m_Height); // OnResize ha gia' fissato m_Width/m_Height
		m_Tonemap = std::make_unique<ComputePipeline>("shaders/spv/Tonemap.spv",
			std::vector<VkDescriptorType>{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
			static_cast<uint32_t>(sizeof(TonemapParams)));

		VkQueryPoolCreateInfo queryInfo{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
		queryInfo.queryCount = 4;
		vkCreateQueryPool(vk.Device(), &queryInfo, nullptr, &m_Timestamps);
	}

	// Il frame scorso e' finito (BeginFrame ha aspettato la fence), quindi i risultati ci sono.
	void Renderer::ReadTimestamps() {
		if (!m_TimestampsWritten)
			return;

		VulkanContext& vk = VulkanContext::Get();
		uint64_t ticks[4] = {};
		vkGetQueryPoolResults(vk.Device(), m_Timestamps, 0, m_DenoiseWritten ? 4 : 2, sizeof(ticks), ticks, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

		const float toMs = vk.TimestampPeriod() * 1e-6f;
		m_PathTraceMs = static_cast<float>(ticks[1] - ticks[0]) * toMs;
		m_DenoiseMs = m_DenoiseWritten ? static_cast<float>(ticks[3] - ticks[2]) * toMs : 0.0f;
	}

	void Renderer::Render(const Camera& camera, const World& world) {
		if (m_Width == 0 || m_Height == 0)
			return;

		VkCommandBuffer cmd = VulkanContext::Get().Cmd();

		ReadTimestamps();

		// Il dispatch del picking era nel frame scorso: solo ora il suo risultato e' leggibile.
		if (m_PickPending) {
			m_PathTracer->ReadPickResult(m_PickType, m_PickIndex, m_PickDistance);
			m_PickPending = false;
			m_PickAvailable = true;
		}

		// Render scale: mentre la camera si muove si renderizza a risoluzione ridotta (meno
		// raggi -> piu' fluido), poi da fermi si torna a piena risoluzione e riparte l'accumulo.
		// L'immagine ridotta viene poi upscalata dal sampler del tonemap sull'intero viewport.
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

		// Ping-pong: si scrive nell'indice corrente e si legge la storia dall'altro. Alla fine
		// del frame gli indici si scambiano, quindi la storia non costa nessuna copia.
		const int cur = m_Ping, prev = 1 - m_Ping;

		glm::ivec2 pickPixel = m_PickRequested ? glm::ivec2(m_PickX, m_PickY) : glm::ivec2(-1, -1);

		// Senza riproiezione si torna al comportamento classico: la camera che si muove invalida
		// l'accumulo e si riparte da capo. E' esattamente cio' che la riproiezione evita, quindi
		// il toggle serve solo come termine di paragone.
		if (!Reproject && m_Interacting)
			ResetPathTracingCounter();

		// La storia non e' valida al primo frame dopo un reset (cambio scena/materiale/risoluzione):
		// m_PTCounter torna a 1 e i buffer contengono ancora il mondo precedente. Da fermo la
		// riproiezione e' l'identita', quindi questo path copre anche l'accumulo classico.
		bool hasHistory = m_PTCounter > 1;
		bool cameraMoved = m_Interacting;

		// Gli AOV dipendono solo dalla camera: si ritracciano solo se e' cambiata davvero.
		// Il confronto e' sulle matrici, non su Camera::OnUpdate: quello riporta il movimento
		// da tastiera, ma la rotazione col mouse cambia comunque la vista e lascerebbe AOV
		// stantii. Da fermo questo risparmia un raggio primario per pixel.
		glm::mat4 viewProjection = camera.GetProjection() * camera.GetView();
		bool aovNeedsUpdate = (viewProjection != m_PrevViewProjection) || m_PTCounter <= 1 || m_sceneReset;

		ComputeUniformContainer container = { static_cast<uint32_t>(m_PTCounter), static_cast<uint32_t>(m_SamplesPerPixel), static_cast<uint32_t>(m_RayDepth), m_sceneReset, EnvironmentMapping, FireflyClamp, Aperture, FocusDistance, AOVView, m_PrevViewProjection, m_PrevCameraPosition, aovNeedsUpdate, hasHistory, cameraMoved, HistoryCap, NEE, pickPixel, *m_World, *m_Camera };

		// Le scritture del frame scorso diventano visibili a questo, e il tonemap non deve
		// sovrascrivere il display mentre ImGui lo sta ancora leggendo.
		{
			VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
			VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
			dependency.memoryBarrierCount = 1;
			dependency.pMemoryBarriers = &barrier;
			vkCmdPipelineBarrier2(cmd, &dependency);
		}

		vkCmdResetQueryPool(cmd, m_Timestamps, 0, 4);
		vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, m_Timestamps, 0);

		m_PathTracer->Dispatch(cmd, container, { m_RenderedImage, m_AlbedoAOV, m_NormalDepth[cur], m_Irradiance[cur], m_NormalDepth[prev], m_Irradiance[prev] });

		vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, m_Timestamps, 1);
		m_TimestampsWritten = true;
		ComputeBarrier(cmd);

		if (m_PickRequested) {
			m_PickRequested = false;
			m_PickPending = true;
		}

		// Denoiser à-trous (view filter): gira solo sulla vista Beauty, non in debug AOV.
		// L'output rimpiazza m_RenderedImage come sorgente del tonemap.
		// L'auto-fade ora vive nello shader ed e' PER-PIXEL (ogni pixel ha il suo N): qui resta
		// solo il peso manuale.
		//
		// Skip: le 5 passate full-screen sono frame rubati al path tracer, che accumula piu'
		// lentamente e quindi RESTA piu' rumoroso. Si puo' saltare solo se il fade e' trascurabile
		// OVUNQUE. Il minimo N sullo schermo e' ~m_FramesSinceMove: i pixel disoccclusi allo stop
		// sono i piu' indietro e crescono di 1 al frame. In movimento non si salta mai.
		m_FramesSinceMove = m_Interacting ? 0 : m_FramesSinceMove + 1;
		float minFade = 1.0f;
		if (DenoiseAutoFade && m_FramesSinceMove > 1)
			minFade = std::clamp(1.0f / std::sqrt(static_cast<float>(m_FramesSinceMove)), 0.0f, 1.0f);

		const GpuImage* source = &m_RenderedImage;
		m_DenoiseWritten = Denoise && AOVView == 0 && DenoiseStrength * minFade > 0.01f;
		if (m_DenoiseWritten) {
			vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, m_Timestamps, 2);
			source = &m_Denoiser->Run(cmd, m_Irradiance[cur], m_RenderedImage, m_AlbedoAOV, m_NormalDepth[cur],
			                          DenoiseStrength, DenoiseAutoFade, DenoiseCPhi, DenoiseNPhi, DenoisePPhi, DenoiseAPhi);
			vkCmdWriteTimestamp2(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, m_Timestamps, 3);
		}

		const TonemapParams tonemap = { Exposure, PostProcessing ? 1 : 0, AOVView };
		m_Tonemap->Bind(cmd);
		m_Tonemap->Sampler(0, *source, m_DisplaySampler, VK_IMAGE_LAYOUT_GENERAL).Image(1, m_Display).Commit(cmd);
		m_Tonemap->PushConstants(cmd, &tonemap, sizeof(tonemap));
		vkCmdDispatch(cmd, (m_Width + 7) / 8, (m_Height + 7) / 8, 1);

		// ImGui campiona il display nel fragment shader.
		{
			VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
			dependency.memoryBarrierCount = 1;
			dependency.pMemoryBarriers = &barrier;
			vkCmdPipelineBarrier2(cmd, &dependency);
		}

		// Il frame corrente diventa la storia del prossimo: si scambiano gli indici e si
		// memorizza la camera con cui e' stato prodotto (serve a riproiettare e a validare).
		m_Ping = 1 - m_Ping;
		m_PrevViewProjection = camera.GetProjection() * camera.GetView();
		m_PrevCameraPosition = camera.GetPosition();

		UpdateAccumulationCounter();
	}

	void Renderer::UpdateAccumulationCounter() {
		if (PathTracing)
			m_PTCounter++;
		else
			m_PTCounter = 1;

		if (m_sceneReset)
			m_sceneReset = false;
	}

	void Renderer::ReadImage(const GpuImage& image, void* pixels, size_t bytes) {
		VulkanContext& vk = VulkanContext::Get();
		vkDeviceWaitIdle(vk.Device());

		GpuBuffer staging = vk.CreateBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
		vk.Immediate([&](VkCommandBuffer cmd) {
			VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
			VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
			dependency.memoryBarrierCount = 1;
			dependency.pMemoryBarriers = &barrier;
			vkCmdPipelineBarrier2(cmd, &dependency);

			VkBufferImageCopy region{};
			region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.imageExtent = { image.Width, image.Height, 1 };
			vkCmdCopyImageToBuffer(cmd, image.Image, VK_IMAGE_LAYOUT_GENERAL, staging.Buffer, 1, &region);
		});
		memcpy(pixels, staging.Mapped, bytes);
		vk.DestroyBuffer(staging);
	}

	// Il contenuto e' gia' passato dal tonemap, quindi il file corrisponde a cio' che si vede.
	std::string Renderer::SaveScreenshot(const std::string& path) {
		std::string fileName = path;
		if (fileName.empty()) {
			std::filesystem::create_directories("screenshots");
			fileName = "screenshots/" + std::to_string(screenshotsCount++) + ".png";
		} else {
			std::filesystem::path p(fileName);
			if (p.has_parent_path())
				std::filesystem::create_directories(p.parent_path());
		}

		std::vector<unsigned char> pixels(static_cast<size_t>(m_Display.Width) * m_Display.Height * 4);
		ReadImage(m_Display, pixels.data(), pixels.size());

		// riga 0 = NDC y -1, cioe' il fondo dell'immagine: i PNG partono dall'alto
		stbi_flip_vertically_on_write(true);
		if (!stbi_write_png(fileName.c_str(), m_Display.Width, m_Display.Height, 4, pixels.data(), m_Display.Width * 4)) {
			std::cout << "Screenshot: scrittura fallita: " << fileName << std::endl;
			return "";
		}

		std::cout << "Screenshot: " << fileName << " (" << m_Display.Width << "x" << m_Display.Height << ")" << std::endl;
		return fileName;
	}

	void Renderer::SaveHdr(const std::string& path) {
		const int width = static_cast<int>(m_RenderedImage.Width), height = static_cast<int>(m_RenderedImage.Height);
		std::vector<float> pixels(static_cast<size_t>(width) * height * 4);
		ReadImage(m_RenderedImage, pixels.data(), pixels.size() * sizeof(float));

		std::ofstream file(path, std::ios::binary);
		file.write(reinterpret_cast<const char*>(&width), sizeof(int));
		file.write(reinterpret_cast<const char*>(&height), sizeof(int));
		file.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(float));
	}

	void Renderer::Release() {
		VulkanContext& vk = VulkanContext::Get();
		m_PathTracer.reset();
		m_Denoiser.reset();
		m_Tonemap.reset();
		for (GpuImage* image : { &m_RenderedImage, &m_AlbedoAOV, &m_NormalDepth[0], &m_NormalDepth[1], &m_Irradiance[0], &m_Irradiance[1], &m_Display })
			vk.DestroyImage(*image);
		if (m_Timestamps != VK_NULL_HANDLE)
			vkDestroyQueryPool(vk.Device(), m_Timestamps, nullptr);
		if (m_DisplaySet != VK_NULL_HANDLE)
			ImGui_ImplVulkan_RemoveTexture(m_DisplaySet);
		if (m_DisplaySampler != VK_NULL_HANDLE)
			vkDestroySampler(vk.Device(), m_DisplaySampler, nullptr);
		m_Timestamps = VK_NULL_HANDLE;
		m_DisplaySampler = VK_NULL_HANDLE;
		m_DisplaySet = VK_NULL_HANDLE;
	}
}
