#pragma once

#ifndef World_h__
#define World_h__

#include "Ray.h"
#include <vector>
#include <glm/glm.hpp>
#include <cmath>

enum class ObjectType {
	SPHERE,
	QUAD,
	BACKGROUND
};

struct Sphere {
	glm::vec3 Position{0.0f};
	float Radius = 1.0f;
	glm::vec4 Color{0.0f};

	float Hit(const Ray& ray) const {
		glm::vec3 oc = ray.Origin - Position;

		float a = glm::dot(ray.Direction, ray.Direction);
		float b = 2.0f * glm::dot(oc, ray.Direction);
		float c = glm::dot(oc, oc) - Radius * Radius;
		float discriminant = b * b - 4.0f * a * c;

		if (discriminant < 0.0f)
			return -1.0f;

		float closestHit = (-b - glm::sqrt(discriminant)) / (2.0f * a);

		return closestHit;
	}
};

struct Quad {
	glm::vec3 PositionLLC{0.0f};
	glm::vec3 U{0.0f};
	glm::vec3 V{0.0f};
	glm::vec4 Color{0.0f};

	float Width = 1.0f;
	float Height = 1.0f;

	float Hit(const Ray& ray) const {
		glm::vec3 normal = glm::normalize(glm::cross(U, V));
		double d = glm::dot(normal, PositionLLC);
		glm::vec3 w = normal / glm::dot(normal, normal);

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

	/*
	float Hit(const Ray& ray) const {
		glm::vec3 toQuad = PositionLLC - ray.Origin;
		glm::vec3 normal = glm::normalize(glm::cross(U, V));

		float t = glm::dot(toQuad, normal) / glm::dot(ray.Direction, normal);

		if (t < 0.0f) {
			return -1.0f;  // Il raggio è dietro il quadrato
		}

		glm::vec3 hitPoint = ray.Origin + t * ray.Direction;

		glm::vec3 toHit = hitPoint - PositionLLC;
		float u = glm::dot(toHit, U);
		float v = glm::dot(toHit, V);

		if (u >= 0 && u <= glm::length(U) && v >= 0 && v <= glm::length(V)) {
			return t;  // Il raggio colpisce il quadrato
		}

		return -1.0f;  // Il raggio non colpisce il quadrato
	}
	*/
};

struct World {
	glm::vec4 BackgroundColor;
	std::vector<Sphere> Spheres;
	std::vector<Quad> Quads;
};

#endif // World_h__