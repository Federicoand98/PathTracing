#pragma once

#ifndef RANDOM_H
#define RANDOM_H

#include "ptpch.h"

namespace PathTracer {

	class Random {
	public:
		static int GetInt(int min, int max) {
			static std::random_device rd;
			static std::mt19937 gen(rd());
			std::uniform_int_distribution<int> dist(min, max);

			return dist(gen);
		}

		static float GetFloat(float min, float max) {
			static std::random_device rd;
			static std::mt19937 gen(rd());
			std::uniform_real_distribution<float> dist(min, max);

			return dist(gen);
		}

		static glm::vec3 GetVec3(float min, float max) {
			static std::random_device rd;
			static std::mt19937 gen(rd());
			std::uniform_real_distribution<float> dist(min, max);
			
			float x = dist(gen);
			float y = dist(gen);
			float z = dist(gen);

			return glm::vec3(x, y, z);
		}
	};
}

#endif // RANDOM_H

