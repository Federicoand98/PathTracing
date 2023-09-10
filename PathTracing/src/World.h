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

struct Sphere2 {
	glm::vec4 Position{0.0f};
	glm::vec4 Mat{0.0f};
};

struct Sphere {
	glm::vec3 Position{0.0f};
	float Radius = 1.0f;
	int MaterialIndex = 0;
};

struct Quad {
	glm::vec3 PositionLLC{0.0f};
	glm::vec3 U{0.0f};
	glm::vec3 V{0.0f};
	int MaterialIndex = 0;

	float Width = 1.0f;
	float Height = 1.0f;

	float Hit(const Ray& ray) const {
		glm::vec3 normal = glm::normalize(glm::cross(U, V));
		double d = glm::dot(normal, PositionLLC);
		glm::vec3 w = normal / (glm::dot(normal, normal) + 0.0001f);

		auto denom = glm::dot(normal, ray.Direction);
		
		// no hit if the ray is parallel to the plane
		if (fabs(denom) < 1e-8)
			return -1.0f;

		float t = (d - glm::dot(normal, ray.Origin)) / denom;
		if (t < 0.001 || t > INFINITY)
			return -1.0f;

		auto intersection = ray.Origin + (t * ray.Direction);
		glm::vec3 planar_hitpt_vector = intersection - PositionLLC;
		auto alpha = glm::dot(w, glm::cross(planar_hitpt_vector, V));
		auto beta = glm::dot(w, glm::cross(U, planar_hitpt_vector));

		// is not interior
		if ((alpha < 0) || (Width < alpha) || (beta < 0) || (Height < beta))
			return -1;

		return t;
	}
};

enum class SceneType {
	TWO_SPHERES = 0,
	RANDOM_SPHERES = 1
};

class World {
public:
	World();
	~World();

	void LoadScene();
	void DestroyScene();
public:
	glm::vec3 BackgroundColor;
	std::vector<Sphere2> Spheres;
	std::vector<Material> Materials;
	float AmbientOcclusionIntensity = 1.0f;
	int CurrentScene = 1;
private:
	void PrepareMaterials();
	void PrepareSimpleScene();
	void PrepareRandomScene();
};

#endif // WORLD_H