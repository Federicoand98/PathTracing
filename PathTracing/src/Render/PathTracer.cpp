#include "Render/PathTracer.h"

#include <cstring>

namespace Lucerna {

	namespace {
		// Layout dei descrittori di PathTracing.comp, nell'ordine dei binding.
		const std::vector<VkDescriptorType> BINDINGS = {
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		};

		// Layout del PickBuffer: due int + un float, tutti scalari 4 byte. La sentinella e' -2,
		// non -1: cosi' "lo shader non ha scritto" (-2) si distingue da "raggio a vuoto" (-1).
		struct PickData { int type; int index; float distance; };
	}

	PathTracer::PathTracer() {
		VulkanContext& vk = VulkanContext::Get();

		m_Frame = vk.CreateBuffer(sizeof(FrameParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		m_Pick = vk.CreateBuffer(sizeof(PickData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		const PickData empty = { -2, -2, -1.0f };
		memcpy(m_Pick.Mapped, &empty, sizeof(empty));

		m_Pipeline = std::make_unique<ComputePipeline>("shaders/spv/PathTracing.spv", BINDINGS);
	}

	PathTracer::~PathTracer() {
		VulkanContext& vk = VulkanContext::Get();
		vk.DestroyBuffer(m_Pick);
		vk.DestroyBuffer(m_Frame);
	}

	void PathTracer::Dispatch(VkCommandBuffer cmd, const ComputeUniformContainer& container, const PathTracerTargets& targets) {
		const RenderScene& scene = container.Scene;

		if (container.PickPixel.x >= 0) {
			const PickData empty = { -2, -2, -1.0f };
			memcpy(m_Pick.Mapped, &empty, sizeof(empty));
		}

		FrameParams frame{};
		frame.InverseProjection = container.Camera.InverseProjection;
		frame.InverseView = container.Camera.InverseView;
		frame.PrevViewProjection = container.PrevViewProjection;
		frame.CameraPosition = container.Camera.Position;
		frame.RendererFrame = static_cast<int>(container.NumFrames);
		frame.PrevCameraPosition = container.PrevCameraPosition;
		frame.SamplesPerPixel = static_cast<int>(container.SamplesPerPixel);
		frame.BackgroundColor = scene.BackgroundColor();
		frame.SunDirection = scene.SunDirection();
		frame.SunRadiance = scene.SunRadiance();
		frame.RayDepth = static_cast<int>(container.RayDepth);
		frame.PickPixel = container.PickPixel;
		frame.AovView = container.AOVView;
		frame.FireflyClamp = container.FireflyClamp;
		frame.ApertureRadius = container.ApertureRadius;
		frame.FocusDistance = container.FocusDistance;
		frame.AovNeedsUpdate = container.AovNeedsUpdate ? 1 : 0;
		frame.HasHistory = container.HasHistory ? 1 : 0;
		frame.CameraMoved = container.CameraMoved ? 1 : 0;
		frame.HistoryCap = container.HistoryCap;
		frame.NumLights = container.EnableNEE ? scene.NumLights() : 0;
		frame.EnvironmentMapping = scene.EnvironmentMapping() ? 1 : 0;
		frame.NumSpheres = scene.NumSpheres();
		frame.NumMaterials = scene.NumMaterials();
		memcpy(m_Frame.Mapped, &frame, sizeof(frame));

		m_Pipeline->Bind(cmd);
		m_Pipeline->Image(0, targets.Output).Image(1, targets.Albedo).Image(2, targets.NormalDepth)
			.Image(3, targets.Irradiance).Image(4, targets.NormalDepthPrev).Image(5, targets.IrradiancePrev)
			.Sampler(6, scene.Environment(), scene.EnvironmentSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			.Sampler(7, scene.AlbedoTextures().Image(), scene.AlbedoTextures().Sampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			.Buffer(8, m_Frame).Buffer(9, scene.Spheres()).Buffer(10, scene.Materials()).Buffer(11, scene.Quads())
			.Buffer(12, scene.TriPositions()).Buffer(13, scene.Meshes()).Buffer(14, scene.TriNormals()).Buffer(15, m_Pick)
			.Buffer(16, scene.TriUVs()).Buffer(17, scene.Lights()).AccelerationStructure(18, scene.TopLevel())
			.Sampler(19, scene.DataTextures().Image(), scene.DataTextures().Sampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			.Commit(cmd);

		// ceil: con risoluzioni non multiple di 8 (es. render scale) la divisione intera
		// perderebbe i pixel di bordo. I thread in eccesso escono subito (bounds-guard nello shader).
		vkCmdDispatch(cmd, (targets.Output.Width + 7) / 8, (targets.Output.Height + 7) / 8, 1);
	}

	void PathTracer::ReadPickResult(int& objectType, int& objectIndex, float& distance) const {
		PickData result;
		memcpy(&result, m_Pick.Mapped, sizeof(result));
		objectType = result.type;
		objectIndex = result.index;
		distance = result.distance;
	}
}
