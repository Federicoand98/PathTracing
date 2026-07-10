#pragma once

#ifndef WORLD_H
#define WORLD_H

#include "ptpch.h"

#include "Ray.h"
#include "Renderer/Material.h"
#include "Renderer/Primitives.h"
#include "Graphics/Model.h"
#include "Graphics/BVH.h"
#include "Graphics/BVHBuilder.h"

namespace PathTracer {



	enum class SceneType {
		TWO_SPHERES = 0,
		RANDOM_SPHERES = 1,
		CORNELL_BOX = 2,
		RANDOM_BOXES = 3,
		CORNELL_BOX_MESH = 4,
		SETUP_1 = 5,
		SETUP_2 = 6,
		SETUP_3 = 7
	};

	class World {
	public:
		World();
		~World();

		void LoadScene();
		void DestroyScene();
		void SetMeshTransform(int meshIndex, const glm::mat4& transform);

		// Creazione a runtime. Sono tutte APPEND: gli indici gia' in giro (selezione,
		// MaterialIndex degli oggetti, Box::index) restano validi.
		void CreateBox(const glm::vec3& a, const glm::vec3& b, float MaterialIndex);
		int AddMesh(const std::string& objPath, const glm::vec3& position, int material);
	public:
		glm::vec3 BackgroundColor;
        std::vector<Test> Tests;
		std::vector<Sphere> Spheres;
		std::vector<Quad> Quads;
		std::vector<Box> Boxes;
		std::vector<Material> Materials;
		std::vector<MeshInfo> Meshes;
		std::vector<std::string> MeshNames;       // solo CPU, per la UI
		std::vector<Triangle> Triangles;          // authoring CPU, sorgente per il BVH
		std::vector<TrianglePosition> TriPositions; // caricati sulla GPU (binding 3)
		std::vector<TriangleNormal> TriNormals;    // caricate sulla GPU (binding 8)
		std::vector<TriangleUV> TriUVs;            // caricate sulla GPU (binding 10)
		std::vector<std::string> TexturePaths;     // un layer del sampler2DArray ciascuna
        std::vector<BVHNode> Nodes;
		std::vector<BVHNodeAlt> NodesAlt;
		std::vector<int> TriIndex;
		std::vector<BVHNodeNew> BVHNodes;
		BVHNodeNew* root;
		std::unique_ptr<BVH> bvh;
		int CurrentScene = 0;
	private:
		void PrepareMaterials();
		void PrepareSimpleScene();
		void PrepareCornellBox();
		void PrepareCornellBoxMesh();
		void PrepareRandomSpheres();
		void PrepareRandomBoxes();
		void PrepareSetup1();
		void PrepareSetup2();
		void PrepareSetup3();

		void UploadModel(const Model& model, const glm::vec3& Position, int material);
		void BuildBVH();
		int ComputeBVHDepth(int rootNode) const;

		void CreateCornellBox();
	};
}

#endif // WORLD_H