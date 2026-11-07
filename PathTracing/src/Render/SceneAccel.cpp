#include "Render/SceneAccel.h"

#include <cstring>

namespace Lucerna {

	namespace {
		// Le funzioni delle estensioni non stanno nel loader: vanno chieste al device.
		struct Functions {
			PFN_vkGetAccelerationStructureBuildSizesKHR GetBuildSizes;
			PFN_vkCreateAccelerationStructureKHR Create;
			PFN_vkDestroyAccelerationStructureKHR Destroy;
			PFN_vkCmdBuildAccelerationStructuresKHR CmdBuild;
			PFN_vkGetAccelerationStructureDeviceAddressKHR GetAddress;
		};

		const Functions& Fn() {
			static const Functions functions = [] {
				VkDevice device = VulkanContext::Get().Device();
				Functions f;
				f.GetBuildSizes = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
				f.Create = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
				f.Destroy = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
				f.CmdBuild = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
				f.GetAddress = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
				return f;
			}();
			return functions;
		}

		const VkMemoryPropertyFlags DEVICE_MEMORY = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		const VkBufferUsageFlags STORAGE_USAGE = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		const VkBufferUsageFlags INPUT_USAGE = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

		VkDeviceSize AlignUp(VkDeviceSize value, VkDeviceSize alignment) {
			return (value + alignment - 1) & ~(alignment - 1);
		}

		VkAccelerationStructureKHR CreateStructure(const GpuBuffer& storage, VkDeviceSize offset, VkDeviceSize size, VkAccelerationStructureTypeKHR type) {
			VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			createInfo.buffer = storage.Buffer;
			createInfo.offset = offset;
			createInfo.size = size;
			createInfo.type = type;
			VkAccelerationStructureKHR structure;
			Fn().Create(VulkanContext::Get().Device(), &createInfo, nullptr, &structure);
			return structure;
		}

		void EnsureBuffer(GpuBuffer& buffer, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory) {
			if (buffer.Size >= size)
				return;
			VulkanContext& vk = VulkanContext::Get();
			vk.DestroyBuffer(buffer);
			buffer = vk.CreateBuffer(size, usage, memory);
		}
	}

	SceneAccel::SceneAccel() {
		VkPhysicalDeviceAccelerationStructurePropertiesKHR properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };
		VkPhysicalDeviceProperties2 properties2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
		properties2.pNext = &properties;
		vkGetPhysicalDeviceProperties2(VulkanContext::Get().PhysicalDevice(), &properties2);
		m_ScratchAlignment = properties.minAccelerationStructureScratchOffsetAlignment;
	}

	SceneAccel::~SceneAccel() {
		VulkanContext& vk = VulkanContext::Get();
		DestroyMeshes();
		if (m_ProceduralBlas != VK_NULL_HANDLE) Fn().Destroy(vk.Device(), m_ProceduralBlas, nullptr);
		if (m_Tlas != VK_NULL_HANDLE) Fn().Destroy(vk.Device(), m_Tlas, nullptr);
		for (GpuBuffer* buffer : { &m_MeshStorage, &m_ProceduralStorage, &m_Aabbs, &m_TlasStorage, &m_Instances, &m_Scratch })
			vk.DestroyBuffer(*buffer);
	}

	void SceneAccel::DestroyMeshes() {
		for (VkAccelerationStructureKHR blas : m_MeshBlas)
			if (blas != VK_NULL_HANDLE)
				Fn().Destroy(VulkanContext::Get().Device(), blas, nullptr);
		m_MeshBlas.clear();
	}

	VkDeviceAddress SceneAccel::AddressOf(VkAccelerationStructureKHR structure) const {
		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
		addressInfo.accelerationStructure = structure;
		return Fn().GetAddress(VulkanContext::Get().Device(), &addressInfo);
	}

	void SceneAccel::BuildMeshes(const World& world, const GpuBuffer& positions) {
		VulkanContext& vk = VulkanContext::Get();
		DestroyMeshes();

		const size_t meshCount = world.Meshes.size();
		m_MeshBlas.assign(meshCount, VK_NULL_HANDLE);

		// Fino a due geometrie per mesh, in quest'ordine: i triangoli opachi e quelli con un
		// materiale mascherato (ADR 0008). Lo shader ricava l'indice del triangolo dall'indice
		// di geometria: la geometria 1 parte da NumOpaque, la 0 da zero.
		std::vector<std::array<VkAccelerationStructureGeometryKHR, 2>> geometries(meshCount);
		std::vector<VkAccelerationStructureBuildGeometryInfoKHR> infos(meshCount);
		std::vector<std::array<VkAccelerationStructureBuildRangeInfoKHR, 2>> ranges(meshCount);
		std::vector<VkDeviceSize> storageOffsets(meshCount), storageSizes(meshCount), scratchOffsets(meshCount);

		VkDeviceSize storageBytes = 0, scratchBytes = 0;
		std::vector<uint32_t> built;

		for (size_t i = 0; i < meshCount; i++) {
			const MeshInfo& mesh = world.Meshes[i];
			const uint32_t triangles = static_cast<uint32_t>(mesh.NumTriangles);
			if (triangles == 0)
				continue;

			const uint32_t opaque = static_cast<uint32_t>(mesh.NumOpaque);
			uint32_t counts[2];
			uint32_t geometryCount = 0;
			for (int part = 0; part < 2; part++) {
				const uint32_t first = part == 0 ? 0 : opaque;
				const uint32_t count = part == 0 ? opaque : triangles - opaque;
				if (count == 0)
					continue;

				// TrianglePosition e' tre vec4 consecutivi: triangoli non indicizzati con stride 16.
				VkAccelerationStructureGeometryKHR& geometry = geometries[i][geometryCount];
				geometry = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
				geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
				// la geometria mascherata non e' opaca: i suoi candidati passano dallo shader
				geometry.flags = part == 0 ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;
				geometry.geometry.triangles = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
				geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
				geometry.geometry.triangles.vertexData.deviceAddress = positions.Address
					+ static_cast<VkDeviceSize>(static_cast<uint32_t>(mesh.FirstTriangle) + first) * sizeof(TrianglePosition);
				geometry.geometry.triangles.vertexStride = sizeof(glm::vec4);
				geometry.geometry.triangles.maxVertex = count * 3 - 1;
				geometry.geometry.triangles.indexType = VK_INDEX_TYPE_NONE_KHR;

				counts[geometryCount] = count;
				ranges[i][geometryCount] = {};
				ranges[i][geometryCount].primitiveCount = count;
				geometryCount++;
			}

			VkAccelerationStructureBuildGeometryInfoKHR& info = infos[i];
			info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
			info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
			info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
			info.geometryCount = geometryCount;
			info.pGeometries = geometries[i].data();

			VkAccelerationStructureBuildSizesInfoKHR sizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
			Fn().GetBuildSizes(vk.Device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, counts, &sizes);

			storageOffsets[i] = storageBytes;
			storageSizes[i] = sizes.accelerationStructureSize;
			storageBytes = AlignUp(storageBytes + sizes.accelerationStructureSize, 256); // allineamento richiesto dalle AS
			scratchOffsets[i] = scratchBytes;
			scratchBytes = AlignUp(scratchBytes + sizes.buildScratchSize, m_ScratchAlignment);

			built.push_back(static_cast<uint32_t>(i));
		}

		if (built.empty())
			return;

		EnsureBuffer(m_MeshStorage, storageBytes, STORAGE_USAGE, DEVICE_MEMORY);
		EnsureBuffer(m_Scratch, scratchBytes + m_ScratchAlignment,
		             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, DEVICE_MEMORY);
		const VkDeviceAddress scratchBase = AlignUp(m_Scratch.Address, m_ScratchAlignment);

		std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos;
		std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> rangePointers;
		for (uint32_t i : built) {
			m_MeshBlas[i] = CreateStructure(m_MeshStorage, storageOffsets[i], storageSizes[i], VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
			infos[i].dstAccelerationStructure = m_MeshBlas[i];
			infos[i].scratchData.deviceAddress = scratchBase + scratchOffsets[i];
			buildInfos.push_back(infos[i]);
			rangePointers.push_back(ranges[i].data());
		}

		vk.Immediate([&](VkCommandBuffer cmd) {
			Fn().CmdBuild(cmd, static_cast<uint32_t>(buildInfos.size()), buildInfos.data(), rangePointers.data());
		});
	}

	void SceneAccel::BuildTopLevel(const World& world) {
		VulkanContext& vk = VulkanContext::Get();

		// ---- BLAS procedurale: sfere e quad si muovono dalla UI, quindi si rifa' a ogni reset ----
		if (m_ProceduralBlas != VK_NULL_HANDLE) {
			Fn().Destroy(vk.Device(), m_ProceduralBlas, nullptr);
			m_ProceduralBlas = VK_NULL_HANDLE;
		}

		const std::vector<AABB> bounds = world.PrimitiveBounds();
		if (!bounds.empty()) {
			std::vector<VkAabbPositionsKHR> aabbs(bounds.size());
			for (size_t i = 0; i < bounds.size(); i++)
				aabbs[i] = { bounds[i].min.x, bounds[i].min.y, bounds[i].min.z, bounds[i].max.x, bounds[i].max.y, bounds[i].max.z };

			const VkDeviceSize aabbBytes = aabbs.size() * sizeof(VkAabbPositionsKHR);
			EnsureBuffer(m_Aabbs, aabbBytes, INPUT_USAGE | VK_BUFFER_USAGE_TRANSFER_DST_BIT, DEVICE_MEMORY);
			vk.UploadBuffer(m_Aabbs, aabbs.data(), aabbBytes);

			VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
			geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
			geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
			geometry.geometry.aabbs = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR };
			geometry.geometry.aabbs.data.deviceAddress = m_Aabbs.Address;
			geometry.geometry.aabbs.stride = sizeof(VkAabbPositionsKHR);

			VkAccelerationStructureBuildGeometryInfoKHR info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
			info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
			info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
			info.geometryCount = 1;
			info.pGeometries = &geometry;

			const uint32_t primitives = static_cast<uint32_t>(aabbs.size());
			VkAccelerationStructureBuildSizesInfoKHR sizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
			Fn().GetBuildSizes(vk.Device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, &primitives, &sizes);

			EnsureBuffer(m_ProceduralStorage, sizes.accelerationStructureSize, STORAGE_USAGE, DEVICE_MEMORY);
			EnsureBuffer(m_Scratch, sizes.buildScratchSize + m_ScratchAlignment,
			             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, DEVICE_MEMORY);

			m_ProceduralBlas = CreateStructure(m_ProceduralStorage, 0, sizes.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
			info.dstAccelerationStructure = m_ProceduralBlas;
			info.scratchData.deviceAddress = AlignUp(m_Scratch.Address, m_ScratchAlignment);

			VkAccelerationStructureBuildRangeInfoKHR range{};
			range.primitiveCount = primitives;
			const VkAccelerationStructureBuildRangeInfoKHR* rangePointer = &range;
			vk.Immediate([&](VkCommandBuffer cmd) { Fn().CmdBuild(cmd, 1, &info, &rangePointer); });
		}

		// ---- TLAS ----
		std::vector<VkAccelerationStructureInstanceKHR> instances;
		instances.reserve(m_MeshBlas.size() + 1);

		auto addInstance = [&](VkAccelerationStructureKHR blas, const glm::mat4& transform, uint32_t customIndex, bool masked) {
			VkAccelerationStructureInstanceKHR instance{};
			// da colonne (glm) a righe, 3x4
			const glm::mat4 t = glm::transpose(transform);
			memcpy(&instance.transform, &t, sizeof(instance.transform));
			instance.instanceCustomIndex = customIndex;
			instance.mask = 0xFF;
			// FORCE_OPAQUE scavalcherebbe il flag delle geometrie: si toglie solo dove serve
			instance.flags = masked ? 0 : VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
			instance.accelerationStructureReference = AddressOf(blas);
			instances.push_back(instance);
		};

		for (size_t i = 0; i < m_MeshBlas.size(); i++)
			if (m_MeshBlas[i] != VK_NULL_HANDLE)
				addInstance(m_MeshBlas[i], world.Meshes[i].Transform, static_cast<uint32_t>(i),
				            world.Meshes[i].NumOpaque < world.Meshes[i].NumTriangles);
		if (m_ProceduralBlas != VK_NULL_HANDLE)
			addInstance(m_ProceduralBlas, glm::mat4(1.0f), 0, false);

		const VkDeviceSize instanceBytes = std::max<VkDeviceSize>(instances.size() * sizeof(VkAccelerationStructureInstanceKHR), 64);
		EnsureBuffer(m_Instances, instanceBytes, INPUT_USAGE,
		             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		if (!instances.empty())
			memcpy(m_Instances.Mapped, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));

		VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		geometry.geometry.instances = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
		geometry.geometry.instances.data.deviceAddress = m_Instances.Address;

		VkAccelerationStructureBuildGeometryInfoKHR info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		info.geometryCount = 1;
		info.pGeometries = &geometry;

		const uint32_t instanceCount = static_cast<uint32_t>(instances.size());
		VkAccelerationStructureBuildSizesInfoKHR sizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		Fn().GetBuildSizes(vk.Device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, &instanceCount, &sizes);

		if (m_Tlas != VK_NULL_HANDLE)
			Fn().Destroy(vk.Device(), m_Tlas, nullptr);
		EnsureBuffer(m_TlasStorage, sizes.accelerationStructureSize, STORAGE_USAGE, DEVICE_MEMORY);
		EnsureBuffer(m_Scratch, sizes.buildScratchSize + m_ScratchAlignment,
		             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, DEVICE_MEMORY);

		m_Tlas = CreateStructure(m_TlasStorage, 0, sizes.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR);
		info.dstAccelerationStructure = m_Tlas;
		info.scratchData.deviceAddress = AlignUp(m_Scratch.Address, m_ScratchAlignment);

		VkAccelerationStructureBuildRangeInfoKHR range{};
		range.primitiveCount = instanceCount;
		const VkAccelerationStructureBuildRangeInfoKHR* rangePointer = &range;
		vk.Immediate([&](VkCommandBuffer cmd) { Fn().CmdBuild(cmd, 1, &info, &rangePointer); });
	}
}
