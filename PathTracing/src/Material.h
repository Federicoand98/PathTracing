#pragma once

#ifndef Material_h__
#define Material_h__

#include <glm/glm.hpp>

struct Material {
	glm::vec4 Color {0.0f};
	float Roughness = 1.0f;
	bool Reflective = false;
};

#endif
