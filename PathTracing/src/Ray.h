#pragma once

#ifndef RAY_H
#define RAY_H

#include <glm/glm.hpp>

namespace PathTracer {

	struct Ray {
		glm::vec3 Origin;
		glm::vec3 Direction;
	};
}

#endif // RAY_H
