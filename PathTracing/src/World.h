#pragma once

#ifndef WORLD_H
#define WORLD_H

#include "Ray.h"
#include <vector>
#include <glm/glm.hpp>
#include <cmath>
#include "Material.h"
#include "Model.h"

enum class ObjectType {
	SPHERE,
	QUAD,
	BACKGROUND
};

struct MeshInfo {
	int FirstTriangle;
	int NumTriangles;
	int MaterialIndex = 0;
	glm::vec4 BoundsMin;
	glm::vec4 BoundsMax;
	float paddind = 0.0f;
};

struct Sphere {
	glm::vec4 Position{0.0f}; // (x, y, z, RADIUS)
	float MaterialIndex = 0;
	glm::vec3 padding{ 0.0f };
};

struct Quad {
	glm::vec4 PositionLLC{0.0f};
	glm::vec4 U{0.0f};
	glm::vec4 V{0.0f};
	float MaterialIndex = 0;
	float Width = 1.0f;
	float Height = 1.0f;
	float padding = 0.0f;
};

struct Box {
	glm::vec3 Min{ 0.0f };
	glm::vec3 Max{ 0.0f };
	float MaterialIndex = 0;
	int index = 0;

	void UpdateBox(std::vector<Quad> &Quads) {
		glm::vec3 dx = glm::vec3(Max.x - Min.x, 0, 0);
		glm::vec3 dy = glm::vec3(0, Max.y - Min.y, 0);
		glm::vec3 dz = glm::vec3(0, 0, Max.z - Min.z);

		glm::vec3 nx = glm::normalize(dx);
		glm::vec3 ny = glm::normalize(dy);
		glm::vec3 nz = glm::normalize(dz);

		Quad quad;

		quad.MaterialIndex = MaterialIndex;

		// front
		{
			quad.PositionLLC = {Min.x, Min.y, Max.z, 0};
			quad.U = { nx, 0 };
			quad.V = { ny, 0 };
			quad.Width = dx.x;
			quad.Height = dy.y;

			Quads[index] = quad;
		}

		// right
		{
			quad.PositionLLC = { Max.x, Min.y, Max.z, 0 };
			quad.U = { -nz, 0 };
			quad.V = { ny, 0 };
			quad.Width = dz.z;
			quad.Height = dy.y;

			Quads[index+1] = quad;
		}

		// back
		{
			quad.PositionLLC = { Max.x, Min.y, Min.z, 0 };
			quad.U = { -nx, 0 };
			quad.V = { ny, 0 };
			quad.Width = dx.x;
			quad.Height = dy.y;

			Quads[index + 2] = quad;
		}

		// left
		{
			quad.PositionLLC = { Min.x, Min.y, Min.z, 0 };
			quad.U = { nz, 0 };
			quad.V = { ny, 0 };
			quad.Width = dz.z;
			quad.Height = dy.y;

			Quads[index + 3] = quad;
		}

		// top
		{
			quad.PositionLLC = { Min.x, Max.y, Max.z, 0 };
			quad.U = { nx, 0 };
			quad.V = { -nz, 0 };
			quad.Width = dx.x;
			quad.Height = dz.z;

			Quads[index + 4] = quad;
		}

		// bottom
		{
			quad.PositionLLC = { Min.x, Min.y, Min.z, 0 };
			quad.U = { nx, 0 };
			quad.V = { nz, 0 };
			quad.Width = dx.x;
			quad.Height = dz.z;

			Quads[index + 5] = quad;
		}
	}
};

enum class SceneType {
	TWO_SPHERES = 0,
	RANDOM_SPHERES = 1,
	CORNELL_BOX = 2,
	RANDOM_BOXES = 3
};

class World {
public:
	World();
	~World();

	void LoadScene();
	void DestroyScene();
public:
	glm::vec3 BackgroundColor;
	std::vector<Sphere> Spheres;
	std::vector<Quad> Quads;
	std::vector<Box> Boxes;
	std::vector<Material> Materials;
	std::vector<MeshInfo> Meshes;
	std::vector<Triangle> Triangles;
	float AmbientOcclusionIntensity = 1.0f;
	int CurrentScene = 0;
private:
	void PrepareMaterials();
	void PrepareSimpleScene();
	void PrepareCornellBox();
	void PrepareRandomSpheres();
	void PrepareRandomBoxes();

	void UploadModel(const Model& model, int material);

	void CreateBox(const glm::vec3& a, const glm::vec3& b, float MaterialIndex);
};

#endif // WORLD_H