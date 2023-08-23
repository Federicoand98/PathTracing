#pragma once

#ifndef World_h__
#define World_h__

#include "Ray.h"
#include <vector>
#include <glm/glm.hpp>

struct Sphere {
	glm::vec3 Position{0.0f};
	float Radius = 1.0f;
	glm::vec4 Color{0.0f};

	float Hit(const Ray& ray) const {
		float a = glm::dot(ray.Direction, ray.Direction);
		float b = 2.0f * glm::dot(ray.Origin, ray.Direction);
		float c = glm::dot(ray.Origin, ray.Origin) - Radius * Radius;
		float discriminant = b * b - 4.0f * a * c;

		if (discriminant < 0.0f)
			return -1.0f;

		float closestHit = (-b - glm::sqrt(discriminant)) / (2.0f * a);

		return closestHit;
	}
};

struct World {
	glm::vec4 BackgroundColor;
	std::vector<Sphere> Spheres;
};

#endif // World_h__