#pragma once

#ifndef WORLD_H
#define WORLD_H

#include "ptpch.h"

#include "Ray.h"
#include "Renderer/Material.h"
#include "Renderer/Primitives.h"
#include "Graphics/Model.h"
#include "Graphics/BVH.h"

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
	public:
		glm::vec3 BackgroundColor;
        std::vector<Test> Tests;
		std::vector<Sphere> Spheres;
		std::vector<Quad> Quads;
		std::vector<Box> Boxes;
		std::vector<Material> Materials;
		std::vector<MeshInfo> Meshes;
		std::vector<Triangle> Triangles;
        std::vector<BVHNode> Nodes;
		std::vector<BVHNodeAlt> NodesAlt;
		std::vector<int> TriIndex;
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

		void CreateBox(const glm::vec3& a, const glm::vec3& b, float MaterialIndex);
		void CreateCornellBox();
	};
}

#endif // WORLD_H