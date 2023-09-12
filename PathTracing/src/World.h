#pragma once

#ifndef WORLD_H
#define WORLD_H

#include "Ray.h"
#include <vector>
#include <glm/glm.hpp>
#include <cmath>
#include "Material.h"

enum class ObjectType {
	SPHERE,
	QUAD,
	BACKGROUND
};

struct Sphere {
	glm::vec4 Position{0.0f};
	float Mat = 0;
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

enum class SceneType {
	TWO_SPHERES = 0,
	RANDOM_SPHERES = 1,
	CORNELL_BOX = 2
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
	std::vector<Material> Materials;
	float AmbientOcclusionIntensity = 1.0f;
	int CurrentScene = 0;
private:
	void PrepareMaterials();
	void PrepareSimpleScene();
	void PrepareCornellBox();
	void PrepareRandomScene();
};

#endif // WORLD_H