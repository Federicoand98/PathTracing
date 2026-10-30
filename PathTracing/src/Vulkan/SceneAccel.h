#pragma once

#ifndef SCENEACCEL_H
#define SCENEACCEL_H

#include "VulkanContext.h"
#include "../World.h"

namespace PathTracer {

	/*
	* Strutture di accelerazione per il ray query.
	*
	* - Un BLAS per mesh, in spazio locale: cambia solo con la geometria.
	* - Un BLAS procedurale di AABB per sfere e quad, che stanno gia' in world space.
	* - Una TLAS con un'istanza per mesh e una per le primitive analitiche. Si ricostruisce a
	*   ogni reset: e' cosi' che i transform delle mesh restano modificabili a costo basso.
	*
	* I BLAS delle mesh dividono un solo buffer: con migliaia di mesh, un'allocazione a testa
	* sforerebbe il limite di allocazioni del driver.
	*/
	class SceneAccel {
	public:
		SceneAccel();
		~SceneAccel();

		void BuildMeshes(const World& world, const GpuBuffer& positions);
		void BuildTopLevel(const World& world);

		const VkAccelerationStructureKHR& TopLevel() const { return m_Tlas; }

	private:
		void DestroyMeshes();
		VkDeviceAddress AddressOf(VkAccelerationStructureKHR structure) const;

	private:
		std::vector<VkAccelerationStructureKHR> m_MeshBlas; // uno per MeshInfo, null se la mesh e' vuota
		GpuBuffer m_MeshStorage;

		VkAccelerationStructureKHR m_ProceduralBlas = VK_NULL_HANDLE;
		GpuBuffer m_ProceduralStorage;
		GpuBuffer m_Aabbs;

		VkAccelerationStructureKHR m_Tlas = VK_NULL_HANDLE;
		GpuBuffer m_TlasStorage;
		GpuBuffer m_Instances;

		GpuBuffer m_Scratch;
		VkDeviceSize m_ScratchAlignment = 0;
	};
}

#endif // SCENEACCEL_H
