#pragma once

#include "ptpch.h"
#include "Vulkan/ComputePipeline.h"
#include "Render/RenderScene.h"
#include "Render/RenderView.h"

namespace Lucerna {

	// Deve combaciare con il blocco FrameParams in res/shaders/pt/bindings.glsl (std140).
	struct FrameParams {
		glm::mat4 InverseProjection;
		glm::mat4 InverseView;
		glm::mat4 PrevViewProjection;
		glm::vec3 CameraPosition;
		int RendererFrame;
		glm::vec3 PrevCameraPosition;
		int SamplesPerPixel;
		glm::vec3 BackgroundColor;
		int RayDepth;
		glm::vec4 SunDirection;
		glm::vec4 SunRadiance;
		glm::ivec2 PickPixel;
		int AovView;
		int NumLights;
		float FireflyClamp;
		float ApertureRadius;
		float FocusDistance;
		int AovNeedsUpdate;
		int HasHistory;
		int CameraMoved;
		int HistoryCap;
		uint32_t EnvironmentMapping;
		int NumSpheres;
		int NumMaterials;
		int Padding[2];
	};
	static_assert(sizeof(FrameParams) == 336, "FrameParams deve combaciare con il layout std140 dello shader");

	struct ComputeUniformContainer {
		uint32_t NumFrames;
		uint32_t SamplesPerPixel;
		uint32_t RayDepth;
		float FireflyClamp;    // luminanza massima per campione, <= 0 = off
		float ApertureRadius;  // raggio lente thin-lens, <= 0 = pinhole (niente DOF)
		float FocusDistance;   // distanza del piano di fuoco
		int AOVView;           // 0 = beauty, 1 = albedo, 2 = normal, 3 = depth, 4 = storia
		// Riproiezione temporale (ADR 0002): camera del frame precedente + stato della storia.
		glm::mat4 PrevViewProjection;
		glm::vec3 PrevCameraPosition;
		bool AovNeedsUpdate;   // true = camera cambiata/reset: ritraccia gli AOV, altrimenti rileggili
		bool HasHistory;       // false = primo frame / reset scena / resize: storia non valida
		bool CameraMoved;      // true = storia riproiettata -> si applica il cap
		int HistoryCap;
		bool EnableNEE;        // false = azzera numLights lato shader (NEE spenta)
		glm::ivec2 PickPixel;  // (-1,-1) = nessuna richiesta di picking
		const RenderScene& Scene;   // cosa si guarda: l'unica scena che il path tracer legge
		const RenderView& Camera;   // da dove si guarda
	};

	// Immagini scritte dal path tracer in questo frame, piu' la storia in sola lettura.
	struct PathTracerTargets {
		const GpuImage& Output;
		const GpuImage& Albedo;
		const GpuImage& NormalDepth;
		const GpuImage& Irradiance;
		const GpuImage& NormalDepthPrev;
		const GpuImage& IrradiancePrev;
	};

	class PathTracer {
	public:
		PathTracer();
		~PathTracer();

		// Carica cio' che serve al frame e registra il dispatch sul command buffer.
		void Dispatch(VkCommandBuffer cmd, const ComputeUniformContainer& container, const PathTracerTargets& targets);
		// Il risultato del picking e' pronto solo a frame eseguito: va letto al frame dopo.
		void ReadPickResult(int& objectType, int& objectIndex, float& distance) const;

	private:
		std::unique_ptr<ComputePipeline> m_Pipeline;
		GpuBuffer m_Pick;   // host-visible: la CPU legge il risultato
		GpuBuffer m_Frame;  // host-visible: parametri del frame
	};
}
