#include "Render/RenderScene.h"

#include "stb/stb_image.h"
#include <cstring>

namespace Lucerna {

	namespace {
		// Gli emettitori per la NEE: sfere e quad con materiale emissivo, il Sole, le luci punto e
		// spot. I triangoli emissivi delle mesh sono esclusi (nessuna scena li usa; li campiona il BSDF).
		std::vector<GPULight> CollectLights(const World& world, const std::unordered_map<ObjectId, int>& materialIndex) {
			std::vector<GPULight> lights;

			auto radianceOf = [&](const ObjectId& id, glm::vec3& outRadiance) -> bool {
				const auto found = materialIndex.find(id);
				if (found == materialIndex.end())
					return false;
				const Material& m = world.Materials()[found->second];
				glm::vec3 rad = glm::vec3(m.EmissiveColor) * m.EmissiveStrenght;
				// luminanza Rec.709: scarta gli emissivi neri o a strength nulla
				float lum = glm::dot(rad, glm::vec3(0.2126f, 0.7152f, 0.0722f));
				if (lum <= 1e-4f)
					return false;
				outRadiance = rad;
				return true;
			};

			// Sfere emissive: campionate via cono (solid angle). Meta = (tipo=0, objType=0, indice).
			for (size_t i = 0; i < world.Spheres().size(); i++) {
				glm::vec3 rad;
				if (!radianceOf(world.Spheres()[i].Material, rad))
					continue;
				float r = world.Spheres()[i].Position.w;
				GPULight L;
				L.Emission = glm::vec4(rad, 4.0f * 3.14159265f * r * r); // area (non usata dal cono)
				L.P = glm::vec4(glm::vec3(world.Spheres()[i].Position), r);
				L.Meta = glm::ivec4(0, 0, static_cast<int>(i), 0);
				lights.push_back(L);
			}

			// Quad emissivi: campionati per area. Nel formato dello shader gli spigoli FISICI sono
			// E1 = U*(Width/k), E2 = V*(Height/k) con k = |U x V|, e area = Width*Height/k. Cosi'
			// il campionamento nello shader e' q = LLC + s*E1 + t*E2 con s,t in [0,1].
			for (size_t i = 0; i < world.Quads().size(); i++) {
				glm::vec3 rad;
				if (!radianceOf(world.Quads()[i].Material, rad))
					continue;
				glm::vec3 u = glm::vec3(world.Quads()[i].U);
				glm::vec3 v = glm::vec3(world.Quads()[i].V);
				glm::vec3 cr = glm::cross(u, v);
				float k = glm::length(cr);
				if (k < 1e-8f)
					continue; // quad degenere
				glm::vec3 n = cr / k;
				glm::vec3 e1 = u * (world.Quads()[i].Width / k);
				glm::vec3 e2 = v * (world.Quads()[i].Height / k);
				float area = (world.Quads()[i].Width * world.Quads()[i].Height) / k;
				GPULight L;
				L.Emission = glm::vec4(rad, area);
				L.P = glm::vec4(glm::vec3(world.Quads()[i].PositionLLC), 0.0f);
				L.U = glm::vec4(e1, 0.0f);
				L.V = glm::vec4(e2, 0.0f);
				L.Normal = glm::vec4(n, 0.0f);
				L.Meta = glm::ivec4(1, 1, static_cast<int>(i), 0);
				lights.push_back(L);
			}

			// Il Sole: P = direzione verso la luce + cos(raggio angolare), Emission.a = angolo solido
			// del disco. Meta.y = -1: nessun oggetto in scena lo rappresenta.
			if (world.Sun().Enabled) {
				const glm::vec3 rad = world.Sun().Radiance();
				if (glm::dot(rad, glm::vec3(0.2126f, 0.7152f, 0.0722f)) > 1e-4f) {
					GPULight L;
					L.Emission = glm::vec4(rad, world.Sun().SolidAngle());
					L.P = glm::vec4(world.Sun().Direction(), world.Sun().CosMax());
					L.Meta = glm::ivec4(2, -1, -1, 0);
					lights.push_back(L);
				}
			}

			// Luci punto e spot: Emission.rgb = intensita', P = posizione, U = direzione dello spot
			// con cos(OuterAngle) in w, V.x = cos(InnerAngle). Nessun oggetto le rappresenta.
			for (const PunctualLight& p : world.PunctualLights()) {
				const glm::vec3 intensity = p.Color * p.Intensity;
				if (glm::dot(intensity, glm::vec3(0.2126f, 0.7152f, 0.0722f)) <= 1e-6f)
					continue;
				GPULight L;
				L.Emission = glm::vec4(intensity, 0.0f);
				L.P = glm::vec4(p.Position, 0.0f);
				if (p.Spot) {
					const float outer = glm::radians(glm::clamp(p.OuterAngle, 0.1f, 90.0f));
					const float inner = glm::radians(glm::clamp(p.InnerAngle, 0.0f, glm::degrees(outer)));
					L.U = glm::vec4(glm::normalize(p.Direction), std::cos(outer));
					L.V = glm::vec4(std::cos(inner), 0.0f, 0.0f, 0.0f);
				}
				L.Meta = glm::ivec4(p.Spot ? 4 : 3, -1, -1, 0);
				lights.push_back(L);
			}

			return lights;
		}

		// AABB world di tutte le primitive analitiche: prima le sfere, poi i quad.
		std::vector<AABB> PrimitiveBounds(const World& world) {
			const int nSpheres = static_cast<int>(world.Spheres().size());
			const int nQuads = static_cast<int>(world.Quads().size());

			std::vector<AABB> boxes;
			boxes.reserve(nSpheres + nQuads);

			for (int i = 0; i < nSpheres; i++) {
				const glm::vec3 c = glm::vec3(world.Spheres()[i].Position);
				const float r = world.Spheres()[i].Position.w;
				AABB b;
				b.expand(c - glm::vec3(r));
				b.expand(c + glm::vec3(r));
				boxes.push_back(b);
			}

			for (int i = 0; i < nQuads; i++) {
				// Il quad si estende per Width/|UxV| lungo U e Height/|UxV| lungo V (vedi la
				// parametrizzazione in HitQuad): gli spigoli fisici sono quelli, non U*Width.
				const glm::vec3 u = glm::vec3(world.Quads()[i].U);
				const glm::vec3 v = glm::vec3(world.Quads()[i].V);
				const glm::vec3 llc = glm::vec3(world.Quads()[i].PositionLLC);
				const float k = glm::length(glm::cross(u, v));
				AABB b;
				if (k < 1e-8f) {
					b.expand(llc); // quad degenere: AABB puntiforme, verra' comunque gonfiata sotto
				} else {
					const glm::vec3 e1 = u * (world.Quads()[i].Width / k);
					const glm::vec3 e2 = v * (world.Quads()[i].Height / k);
					b.expand(llc);
					b.expand(llc + e1);
					b.expand(llc + e2);
					b.expand(llc + e1 + e2);
				}
				// Un quad e' PIATTO: su un asse la AABB ha spessore zero. Un gonfiaggio minimo evita
				// i casi degeneri dello slab test con raggi paralleli a quell'asse.
				const glm::vec3 pad(1e-4f);
				b.min -= pad;
				b.max += pad;
				boxes.push_back(b);
			}

			return boxes;
		}

		// Il Material degli asset come lo legge lo shader: stessi campi, layer al posto degli id
		GpuMaterial ToGpu(const Material& m, const std::unordered_map<ObjectId, int>& layer) {
			auto layerOf = [&](const ObjectId& id) {
				const auto found = layer.find(id);
				return found == layer.end() ? -1.0f : static_cast<float>(found->second);
			};
			GpuMaterial g;
			g.Color = m.Color;
			g.Roughness = m.Roughness;
			g.SpecularProbability = m.SpecularProbability;
			g.RefractionRatio = m.RefractionRatio;
			g.EmissiveStrenght = m.EmissiveStrenght;
			g.EmissiveColor = m.EmissiveColor;
			g.SpecularColor = m.SpecularColor;
			g.RefractionProbability = m.RefractionProbability;
			g.RefractionRoughness = m.RefractionRoughness;
			g.RefractionColor = m.RefractionColor;
			g.AlbedoTexture = layerOf(m.AlbedoTexture);
			g.Checker = m.Checker;
			g.CheckerScale = m.CheckerScale;
			g.NoiseType = m.NoiseType;
			g.NoiseScale = m.NoiseScale;
			g.NoiseBlend = m.NoiseBlend;
			g.NoiseOctaves = m.NoiseOctaves;
			g.NoiseTurbulence = m.NoiseTurbulence;
			g.NoiseColorA = m.NoiseColorA;
			g.NoiseColorB = m.NoiseColorB;
			g.Metalness = m.Metalness;
			g.SpecularTint = m.SpecularTint;
			g.NormalTexture = layerOf(m.NormalTexture);
			g.NormalStrength = m.NormalStrength;
			g.NormalFlipY = m.NormalFlipY;
			g.AlphaTexture = layerOf(m.AlphaTexture);
			g.AlphaCutoff = m.AlphaCutoff;
			return g;
		}
	}

	RenderScene::RenderScene() {
		LoadEnvironment();
		// anche senza texture va creato l'array: lo shader campiona sempre
		m_AlbedoTextures.Load({});
		m_DataTextures.Load({}, 1024, VK_FORMAT_R8G8B8A8_UNORM);
	}

	RenderScene::~RenderScene() {
		VulkanContext& vk = VulkanContext::Get();
		for (GpuBuffer* buffer : { &m_Spheres, &m_Materials, &m_Quads, &m_Meshes, &m_Lights, &m_TriPositions, &m_TriNormals, &m_TriUVs })
			vk.DestroyBuffer(*buffer);
		vk.DestroyImage(m_Environment);
		vkDestroySampler(vk.Device(), m_EnvironmentSampler, nullptr);
	}

	void RenderScene::LoadEnvironment() {
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
	void RenderScene::Upload(GpuBuffer& buffer, const void* data, size_t bytes) {
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

	float RenderScene::MaterialIndexOf(const ObjectId& id) const {
		const auto found = m_MaterialIndex.find(id);
		return found == m_MaterialIndex.end() ? -1.0f : static_cast<float>(found->second);
	}

	void RenderScene::Sync(const World& world, uint32_t changes) {
		// Prima gli indici: tutto il resto traduce gli id in posizioni nei buffer. I materiali si
		// aggiungono solo in coda, quindi gli indici gia' caricati restano validi.
		// Un materiale modificato ricarica solo i materiali; uno nuovo cambia anche gli indici
		// che primitive e mesh portano sulla GPU.
		bool materialsAdded = false;
		if (changes & MaterialsChanged) {
			materialsAdded = world.Materials().size() != m_MaterialIndex.size();
			for (size_t i = 0; i < world.Materials().size() && !materialsAdded; i++) {
				const auto found = m_MaterialIndex.find(world.Materials()[i].Id);
				materialsAdded = found == m_MaterialIndex.end() || found->second != static_cast<int>(i);
			}
		}
		if (materialsAdded) {
			m_MaterialIndex.clear();
			for (size_t i = 0; i < world.Materials().size(); i++)
				m_MaterialIndex[world.Materials()[i].Id] = static_cast<int>(i);
		}

		// I layer sono un dettaglio della GPU (issue #26): ogni tipo di texture ha il suo array, e
		// una texture tolta libera il suo. Il decode e' costoso: si ricarica solo se l'elenco cambia.
		if (changes & TexturesChanged) {
			std::vector<std::string> color, data;
			m_TextureLayer.clear();
			for (const auto& texture : world.Textures()) {
				std::vector<std::string>& list = texture.Kind == TextureKind::Color ? color : data;
				m_TextureLayer[texture.Id] = static_cast<int>(list.size());
				list.push_back(texture.Path);
			}
			m_AlbedoTextures.Load(color);
			m_DataTextures.Load(data, 1024, VK_FORMAT_R8G8B8A8_UNORM);
		}

		if (changes & GeometryChanged)
			BuildGeometry(world);
		if ((changes & (GeometryChanged | MeshesChanged)) || materialsAdded)
			BuildMeshInfos(world);
		if (changes & GeometryChanged)
			m_Accel.BuildMeshes(m_AssetRanges, m_TriPositions);

		if ((changes & PrimitivesChanged) || materialsAdded) {
			std::vector<GpuSphere> spheres;
			for (const Sphere& s : world.Spheres())
				spheres.push_back({ s.Position, MaterialIndexOf(s.Material) });
			std::vector<GpuQuad> quads;
			for (const Quad& q : world.Quads())
				quads.push_back({ q.PositionLLC, q.U, q.V, MaterialIndexOf(q.Material), q.Width, q.Height });
			Upload(m_Spheres, spheres.data(), sizeof(GpuSphere) * spheres.size());
			Upload(m_Quads, quads.data(), sizeof(GpuQuad) * quads.size());
			m_NumSpheres = static_cast<int>(spheres.size());
		}
		if (changes & PrimitivesChanged)
			m_Accel.BuildPrimitives(PrimitiveBounds(world));
		if (changes & (GeometryChanged | MeshesChanged | PrimitivesChanged))
			m_Accel.BuildTopLevel(m_MeshInfos, m_InstanceBlas);

		if (changes & (MaterialsChanged | TexturesChanged)) {
			std::vector<GpuMaterial> materials;
			for (const Material& m : world.Materials())
				materials.push_back(ToGpu(m, m_TextureLayer));
			Upload(m_Materials, materials.data(), sizeof(GpuMaterial) * materials.size());
			m_NumMaterials = static_cast<int>(materials.size());
		}

		// la light list dipende dalle primitive emissive, dai loro materiali, dal Sole e dalle luci
		if (changes & (PrimitivesChanged | MaterialsChanged | LightsChanged))
			BuildLights(world);

		if (changes & EnvironmentChanged) {
			m_BackgroundColor = world.BackgroundColor();
			m_EnvironmentMapping = world.EnvironmentMapping();
		}
	}

	// I triangoli degli asset usati da almeno un'istanza, uno dopo l'altro, e un BLAS per asset:
	// due istanze dello stesso asset dividono entrambi (issue #26). Dentro ogni asset i triangoli
	// mascherati vanno in coda e diventano la seconda geometria del BLAS, l'unica non opaca
	// (ADR 0008). Lo slot di un triangolo diventa l'indice del suo materiale nel buffer.
	void RenderScene::BuildGeometry(const World& world) {
		// ponytail: un asset ha una sola partizione, calcolata col materiale della prima istanza per
		// i triangoli senza slot; un'istanza con un materiale mascherato diverso resta opaca.
		// Servirebbe un BLAS per (asset, partizione).
		std::unordered_map<ObjectId, ObjectId> fallback;
		for (const Mesh& mesh : world.Meshes())
			fallback.emplace(mesh.Asset, mesh.Material);

		std::vector<TrianglePosition> positions;
		std::vector<TriangleNormal> normals;
		std::vector<TriangleUV> uvs;
		m_AssetRanges.clear();
		m_AssetBlas.clear();
		for (const auto& asset : world.Assets().Meshes) {
			const auto used = fallback.find(asset.Id);
			if (used == fallback.end() || asset.Triangles.empty())
				continue;

			auto materialOf = [&](const Triangle& t) -> float {
				const int slot = static_cast<int>(t.MaterialSlot);
				if (slot < 0 || slot >= static_cast<int>(asset.MaterialSlots.size()))
					return -1.0f; // il materiale dell'istanza
				return MaterialIndexOf(asset.MaterialSlots[slot]);
			};
			auto masked = [&](const Triangle& t) {
				const float own = materialOf(t);
				const int mi = static_cast<int>(own >= 0.0f ? own : MaterialIndexOf(used->second));
				return mi >= 0 && mi < static_cast<int>(world.Materials().size()) && world.Materials()[mi].AlphaCutoff > 0.0f;
			};

			std::vector<uint32_t> order(asset.Triangles.size());
			for (uint32_t i = 0; i < order.size(); i++)
				order[i] = i;
			const auto opaqueEnd = std::stable_partition(order.begin(), order.end(),
				[&](uint32_t t) { return !masked(asset.Triangles[t]); });

			MeshInfo range;
			range.FirstTriangle = static_cast<float>(positions.size());
			range.NumTriangles = static_cast<float>(order.size());
			range.NumOpaque = static_cast<float>(opaqueEnd - order.begin());
			m_AssetBlas[asset.Id] = static_cast<int>(m_AssetRanges.size());
			m_AssetRanges.push_back(range);

			for (uint32_t i : order) {
				const Triangle& t = asset.Triangles[i];
				positions.push_back({ t.A, t.B, t.C });
				normals.push_back({ t.NormalA, t.NormalB, t.NormalC });
				// A.z porta il materiale del triangolo (-1 = quello dell'istanza); B/C restano liberi
				uvs.push_back({ glm::vec4(t.UVA, materialOf(t), 0.0f), glm::vec4(t.UVB, 0.0f, 0.0f), glm::vec4(t.UVC, 0.0f, 0.0f) });
			}
		}
		Upload(m_TriPositions, positions.data(), sizeof(TrianglePosition) * positions.size());
		Upload(m_TriNormals, normals.data(), sizeof(TriangleNormal) * normals.size());
		Upload(m_TriUVs, uvs.data(), sizeof(TriangleUV) * uvs.size());
	}

	void RenderScene::BuildMeshInfos(const World& world) {
		m_MeshInfos.clear();
		m_InstanceBlas.clear();
		for (const Mesh& mesh : world.Meshes()) {
			const MeshAsset* asset = world.AssetOf(mesh);
			const auto blas = m_AssetBlas.find(mesh.Asset);
			MeshInfo info;
			if (asset != nullptr && blas != m_AssetBlas.end()) {
				const AABB bounds = world.WorldBounds(mesh);
				info = m_AssetRanges[blas->second];
				info.BoundsMin = glm::vec4(bounds.min, 0.0f);
				info.BoundsMax = glm::vec4(bounds.max, 0.0f);
				info.LocalMin = asset->LocalMin;
				info.LocalMax = asset->LocalMax;
			}
			info.Transform = mesh.Transform;
			info.InvTransform = glm::inverse(mesh.Transform);
			info.MaterialIndex = MaterialIndexOf(mesh.Material);
			m_MeshInfos.push_back(info);
			m_InstanceBlas.push_back(blas != m_AssetBlas.end() ? blas->second : -1);
		}
		Upload(m_Meshes, m_MeshInfos.data(), sizeof(MeshInfo) * m_MeshInfos.size());
	}

	void RenderScene::BuildLights(const World& world) {
		const std::vector<GPULight> lights = CollectLights(world, m_MaterialIndex);
		m_NumLights = static_cast<int>(lights.size());
		if (getenv("PT_LIGHTS"))
			std::cout << "PTLIGHTS " << m_NumLights << " spheres=" << world.Spheres().size()
			          << " quads=" << world.Quads().size() << std::endl;
		Upload(m_Lights, lights.data(), sizeof(GPULight) * lights.size());

		m_SunDirection = m_SunRadiance = glm::vec4(0.0f);
		if (world.Sun().Enabled) {
			m_SunDirection = glm::vec4(world.Sun().Direction(), world.Sun().CosMax());
			m_SunRadiance = glm::vec4(world.Sun().Radiance(), world.Sun().SolidAngle());
		}
	}
}
