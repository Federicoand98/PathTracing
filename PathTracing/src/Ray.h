#pragma once

#ifndef RAY_H
#define RAY_H

#include "ptpch.h"

namespace PathTracer {

	struct Ray {
		glm::vec3 Origin;
		glm::vec3 Direction;
	};
}

#endif // RAY_H
