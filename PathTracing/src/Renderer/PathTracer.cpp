#include "PathTracer.h"

#include "../stb_image.h"
#include <cstring>

namespace PathTracer {

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
		};

		// Layout del PickBuffer: due int + un float, tutti scalari 4 byte. La sentinella e' -2,
		// non -1: cosi' "lo shader non ha scritto" (-2) si distingue da "raggio a vuoto" (-1).
		struct PickData { int type; int index; float distance; };
	}

	PathTracer::PathTracer(const class World& world) {
		VulkanContext& vk = VulkanContext::Get();

		m_Frame = vk.CreateBuffer(sizeof(FrameParams), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		m_Pick = vk.CreateBuffer(sizeof(PickData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		const PickData empty = { -2, -2, -1.0f };
		memcpy(m_Pick.Mapped, &empty, sizeof(empty));

		m_Pipeline = std::make_unique<ComputePipeline>("shaders/spv/PathTracing.spv", BINDINGS);
		m_Accel = std::make_unique<SceneAccel>();

		LoadEnvironment();
		UploadScene(world);

		// anche senza texture va creato l'array: lo shader campiona sempre
		m_AlbedoTextures.Load(world.TexturePaths);
		m_LoadedTexturePaths = world.TexturePaths;
	}

	PathTracer::~PathTracer() {
		VulkanContext& vk = VulkanContext::Get();
		m_Accel.reset();
		for (GpuBuffer* buffer : { &m_Spheres, &m_Materials, &m_Quads, &m_TriPositions, &m_TriNormals, &m_TriUVs,
		                           &m_Meshes, &m_Lights, &m_Pick, &m_Frame })
			vk.DestroyBuffer(*buffer);
		vk.DestroyImage(m_Environment);
		vkDestroySampler(vk.Device(), m_EnvironmentSampler, nullptr);
	}

	void PathTracer::LoadEnvironment() {
		VulkanContext& vk = VulkanContext::Get();

		const char* faces[6] = {
			"textures/EnvironmentMap/Temple/posX.png", "textures/EnvironmentMap/Temple/negX.png",
			"textures/EnvironmentMap/Temple/posY.png", "textures/EnvironmentMap/Temple/negY.png",
			"textures/EnvironmentMap/Temple/posZ.png", "textures/EnvironmentMap/Temple/negZ.png",
		};

		std::vector<unsigned char> pixels;
		int size = 0;
		for (int i = 0; i < 6; i++) {
			int width = 0, height = 0, channels = 0;
			unsigned char* face = stbi_load(faces[i], &width, &height, &channels, 4);
			if (face == nullptr) {
				std::cout << "Failed to load " << faces[i] << ": " << stbi_failure_reason() << std::endl;
				width = height = std::max(size, 1);
				pixels.resize(static_cast<size_t>(i + 1) * width * height * 4, 0);
				continue;
			}
			size = width; // le cubemap sono sempre immagini quadrate
			pixels.insert(pixels.end(), face, face + static_cast<size_t>(width) * height * 4);
			stbi_image_free(face);
		}

		m_Environment = vk.CreateImage(VK_FORMAT_R8G8B8A8_UNORM, size, size,
		                               VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		                               6, 1, VK_IMAGE_VIEW_TYPE_CUBE);

		GpuBuffer staging = vk.CreateBuffer(pixels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		memcpy(staging.Mapped, pixels.data(), pixels.size());

		const VkImage image = m_Environment.Image;
		vk.Immediate([&](VkCommandBuffer cmd) {
			auto barrier = [&](VkImageLayout from, VkImageLayout to) {
				VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
				b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
				b.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
				b.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
				b.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
				b.oldLayout = from;
				b.newLayout = to;
				b.image = image;
				b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
				VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
				dependency.imageMemoryBarrierCount = 1;
				dependency.pImageMemoryBarriers = &b;
				vkCmdPipelineBarrier2(cmd, &dependency);
			};
			barrier(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			VkBufferImageCopy copy{};
			copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 6 };
			copy.imageExtent = { static_cast<uint32_t>(size), static_cast<uint32_t>(size), 1 };
			vkCmdCopyBufferToImage(cmd, staging.Buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
			barrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		});
		vk.DestroyBuffer(staging);

		VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		vkCreateSampler(vk.Device(), &samplerInfo, nullptr, &m_EnvironmentSampler);
	}

	// Un buffer vuoto non esiste in Vulkan: una collezione vuota tiene comunque un buffer minimo,
	// e lo shader ne conosce la dimensione vera dai contatori in FrameParams.
	void PathTracer::Upload(GpuBuffer& buffer, const void* data, size_t bytes) {
		VulkanContext& vk = VulkanContext::Get();
		if (buffer.Size < std::max<size_t>(bytes, 16)) {
			vk.DestroyBuffer(buffer);
			buffer = vk.CreateBuffer(std::max<size_t>(bytes, 16),
			                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
			                         | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		}
		if (bytes > 0)
			vk.UploadBuffer(buffer, data, bytes);
	}

	void PathTracer::UploadScene(const class World& world) {
		// I triangoli e i BLAS si rifanno solo se la geometria e' cambiata: questo metodo gira a
		// ogni reset, cioe' anche a ogni tick di uno slider, e su Sponza sono decine di MB.
		const bool geometryChanged = world.GeometryVersion != m_GeometryVersion;
		m_GeometryVersion = world.GeometryVersion;

		Upload(m_Spheres, world.Spheres.data(), sizeof(Sphere) * world.Spheres.size());
		Upload(m_Materials, world.Materials.data(), sizeof(Material) * world.Materials.size());
		Upload(m_Quads, world.Quads.data(), sizeof(Quad) * world.Quads.size());
		Upload(m_Meshes, world.Meshes.data(), sizeof(MeshInfo) * world.Meshes.size());

		if (geometryChanged) {
			Upload(m_TriPositions, world.TriPositions.data(), sizeof(TrianglePosition) * world.TriPositions.size());
			Upload(m_TriNormals, world.TriNormals.data(), sizeof(TriangleNormal) * world.TriNormals.size());
			Upload(m_TriUVs, world.TriUVs.data(), sizeof(TriangleUV) * world.TriUVs.size());
		}

		// Light list per la NEE: ricostruita qui, quindi resta sempre in sync con la scena
		// (questo metodo gira a ogni reset/cambio scena, incluso un edit dell'emissione dalla
		// UI, che azzera il contatore e forza il re-upload).
		const std::vector<GPULight> lights = world.CollectLights();
		m_NumLights = static_cast<int>(lights.size());
		if (getenv("PT_LIGHTS"))
			std::cout << "PTLIGHTS " << m_NumLights << " spheres=" << world.Spheres.size()
			          << " quads=" << world.Quads.size() << std::endl;
		Upload(m_Lights, lights.data(), sizeof(GPULight) * lights.size());

		if (geometryChanged)
			m_Accel->BuildMeshes(world, m_TriPositions);
		m_Accel->BuildTopLevel(world);
	}

	void PathTracer::Dispatch(VkCommandBuffer cmd, const ComputeUniformContainer& container, const PathTracerTargets& targets) {
		const class World& world = container.World;

		// il decode delle immagini e' costoso: si ricarica solo se la scena le ha cambiate
		if (world.TexturePaths != m_LoadedTexturePaths) {
			m_AlbedoTextures.Load(world.TexturePaths);
			m_LoadedTexturePaths = world.TexturePaths;
		}

		if (container.ResetScene || container.NumFrames == 1)
			UploadScene(world);

		if (container.PickPixel.x >= 0) {
			const PickData empty = { -2, -2, -1.0f };
			memcpy(m_Pick.Mapped, &empty, sizeof(empty));
		}

		FrameParams frame{};
		frame.InverseProjection = container.Camera.GetInverseProjection();
		frame.InverseView = container.Camera.GetInverseView();
		frame.PrevViewProjection = container.PrevViewProjection;
		frame.CameraPosition = container.Camera.GetPosition();
		frame.RendererFrame = static_cast<int>(container.NumFrames);
		frame.PrevCameraPosition = container.PrevCameraPosition;
		frame.SamplesPerPixel = static_cast<int>(container.SamplesPerPixel);
		frame.BackgroundColor = world.BackgroundColor;
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
		frame.NumLights = container.EnableNEE ? m_NumLights : 0;
		frame.EnvironmentMapping = container.EnvironmentMapping ? 1 : 0;
		frame.NumSpheres = static_cast<int>(world.Spheres.size());
		frame.NumMaterials = static_cast<int>(world.Materials.size());
		memcpy(m_Frame.Mapped, &frame, sizeof(frame));

		m_Pipeline->Bind(cmd);
		m_Pipeline->Image(0, targets.Output).Image(1, targets.Albedo).Image(2, targets.NormalDepth)
			.Image(3, targets.Irradiance).Image(4, targets.NormalDepthPrev).Image(5, targets.IrradiancePrev)
			.Sampler(6, m_Environment, m_EnvironmentSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			.Sampler(7, m_AlbedoTextures.Image(), m_AlbedoTextures.Sampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			.Buffer(8, m_Frame).Buffer(9, m_Spheres).Buffer(10, m_Materials).Buffer(11, m_Quads)
			.Buffer(12, m_TriPositions).Buffer(13, m_Meshes).Buffer(14, m_TriNormals).Buffer(15, m_Pick)
			.Buffer(16, m_TriUVs).Buffer(17, m_Lights).AccelerationStructure(18, m_Accel->TopLevel())
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
