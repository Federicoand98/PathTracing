#pragma once

#ifndef RENDER_RENDERSCENE_H
#define RENDER_RENDERSCENE_H

#include "ptpch.h"
#include "Scene/Scene.h"
#include "Render/GpuTypes.h"
#include "Render/SceneAccel.h"
#include "Vulkan/TextureArray.h"

namespace Lucerna {

	/*
	* La scena come la consuma la GPU (issue #24): buffer di triangoli, materiali, primitive e
	* mesh, light list, strutture di accelerazione, texture array e cubemap dell'ambiente.
	* Si costruisce dalla Scene e dalla AssetLibrary ed e' l'unica cosa che il path tracer legge;
	* scena e asset non contengono niente di questa forma. E' la divisione descrizione/rappresentazione di Hydra o dell'FScene
	* di Unreal.
	*
	* Si aggiorna solo cio' che scena e asset segnalano come cambiato (issue #25): spostare una mesh
	* rifa' la TLAS, un materiale ricarica i materiali e la light list, e triangoli e BLAS (decine
	* di MB su Sponza) si toccano solo quando cambia la geometria.
	*/
	class RenderScene {
	public:
		RenderScene();
		~RenderScene();
		RenderScene(const RenderScene&) = delete;
		RenderScene& operator=(const RenderScene&) = delete;

		// Prima del frame, con i cambiamenti di scena e asset (SceneChange) dall'ultima volta.
		void Sync(const Scene& scene, const AssetLibrary& assets, uint32_t changes);

		const GpuBuffer& Spheres() const { return m_Spheres; }
		const GpuBuffer& Materials() const { return m_Materials; }
		const GpuBuffer& Quads() const { return m_Quads; }
		const GpuBuffer& Meshes() const { return m_Meshes; }
		const GpuBuffer& TriPositions() const { return m_TriPositions; }
		const GpuBuffer& TriNormals() const { return m_TriNormals; }
		const GpuBuffer& TriUVs() const { return m_TriUVs; }
		const GpuBuffer& Lights() const { return m_Lights; }
		const VkAccelerationStructureKHR& TopLevel() const { return m_Accel.TopLevel(); }
		const TextureArray& AlbedoTextures() const { return m_AlbedoTextures; }
		const TextureArray& DataTextures() const { return m_DataTextures; }
		const GpuImage& Environment() const { return m_Environment; }
		VkSampler EnvironmentSampler() const { return m_EnvironmentSampler; }

		// Cio' che lo shader riceve in FrameParams invece che in un buffer
		int NumLights() const { return m_NumLights; }
		int NumSpheres() const { return m_NumSpheres; }
		int NumMaterials() const { return m_NumMaterials; }
		const glm::vec3& BackgroundColor() const { return m_BackgroundColor; }
		bool EnvironmentMapping() const { return m_EnvironmentMapping; }
		const glm::vec4& SunDirection() const { return m_SunDirection; } // xyz verso il Sole, w = cos del raggio
		const glm::vec4& SunRadiance() const { return m_SunRadiance; }   // rgb, w = angolo solido; zero se spento

	private:
		void BuildGeometry(const Scene& scene, const AssetLibrary& assets);
		void BuildMeshInfos(const Scene& scene, const AssetLibrary& assets);
		void BuildLights(const Scene& scene, const AssetLibrary& assets);
		float MaterialIndexOf(const ObjectId& id) const; // -1 = nessun materiale
		void Upload(GpuBuffer& buffer, const void* data, size_t bytes);
		void LoadEnvironment();

	private:
		GpuBuffer m_Spheres, m_Materials, m_Quads, m_Meshes, m_Lights;
		GpuBuffer m_TriPositions, m_TriNormals, m_TriUVs;
		SceneAccel m_Accel;

		TextureArray m_AlbedoTextures;
		TextureArray m_DataTextures;
		GpuImage m_Environment;
		VkSampler m_EnvironmentSampler = VK_NULL_HANDLE;

		// Gli id diventano posizioni nei buffer solo qui (issue #26)
		std::unordered_map<ObjectId, int> m_MaterialIndex;
		std::unordered_map<ObjectId, int> m_TextureLayer;   // layer nell'array del suo tipo
		std::unordered_map<ObjectId, int> m_AssetBlas;      // asset di geometria -> indice in m_AssetRanges
		std::vector<MeshInfo> m_AssetRanges;  // per asset usato: triangoli e partizione, il suo BLAS
		std::vector<MeshInfo> m_MeshInfos;    // per istanza: l'ingresso della TLAS
		std::vector<int> m_InstanceBlas;      // per istanza: il BLAS del suo asset, -1 = nessuno

		int m_NumLights = 0, m_NumSpheres = 0, m_NumMaterials = 0;
		glm::vec3 m_BackgroundColor{ 0.0f };
		bool m_EnvironmentMapping = false;
		glm::vec4 m_SunDirection{ 0.0f }, m_SunRadiance{ 0.0f };
	};
}

#endif // RENDER_RENDERSCENE_H
