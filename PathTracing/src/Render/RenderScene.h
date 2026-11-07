#pragma once

#ifndef RENDER_RENDERSCENE_H
#define RENDER_RENDERSCENE_H

#include "ptpch.h"
#include "Legacy/World.h"
#include "Render/GpuTypes.h"
#include "Render/SceneAccel.h"
#include "Vulkan/TextureArray.h"

namespace Lucerna {

	/*
	* La scena come la consuma la GPU (issue #24): buffer di triangoli, materiali, primitive e
	* mesh, light list, strutture di accelerazione, texture array e cubemap dell'ambiente.
	* Si costruisce dal World ed e' l'unica cosa che il path tracer legge; il World non contiene
	* niente di questa forma. E' la divisione descrizione/rappresentazione di Hydra o dell'FScene
	* di Unreal.
	*
	* Per ora si ricostruisce tutta a ogni reset; triangoli e BLAS solo quando cambia
	* World::GeometryVersion, perche' su Sponza sono decine di MB. Il change tracking (#25)
	* sostituira' entrambi con aggiornamenti mirati.
	*/
	class RenderScene {
	public:
		RenderScene();
		~RenderScene();
		RenderScene(const RenderScene&) = delete;
		RenderScene& operator=(const RenderScene&) = delete;

		// Prima del frame. reset = la scena e' cambiata: si ricostruisce. Le texture si
		// ricaricano a parte, solo quando cambia l'elenco delle immagini.
		void Sync(const World& world, bool reset);

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
		void Build(const World& world);
		void BuildGeometry(const World& world);
		void Upload(GpuBuffer& buffer, const void* data, size_t bytes);
		void LoadEnvironment();

	private:
		GpuBuffer m_Spheres, m_Materials, m_Quads, m_Meshes, m_Lights;
		GpuBuffer m_TriPositions, m_TriNormals, m_TriUVs;
		SceneAccel m_Accel;

		TextureArray m_AlbedoTextures;
		TextureArray m_DataTextures;
		std::vector<std::string> m_LoadedTexturePaths;
		std::vector<std::string> m_LoadedDataTexturePaths;
		GpuImage m_Environment;
		VkSampler m_EnvironmentSampler = VK_NULL_HANDLE;

		int m_GeometryVersion = -1;      // versione di triangoli e BLAS gia' sulla GPU
		std::vector<float> m_NumOpaque;  // per mesh: la partizione si rifa' solo con la geometria

		int m_NumLights = 0, m_NumSpheres = 0, m_NumMaterials = 0;
		glm::vec3 m_BackgroundColor{ 0.0f };
		bool m_EnvironmentMapping = false;
		glm::vec4 m_SunDirection{ 0.0f }, m_SunRadiance{ 0.0f };
	};
}

#endif // RENDER_RENDERSCENE_H
