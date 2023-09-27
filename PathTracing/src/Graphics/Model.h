#pragma once

#ifndef MODEL_H
#define MODEL_H

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

namespace PathTracer {

	struct Triangle {
		glm::vec4 A;
		glm::vec4 B;
		glm::vec4 C;
		glm::vec4 NormalA;
		glm::vec4 NormalB;
		glm::vec4 NormalC;
	};

	struct Face {
		int vertex_ins[3];
		int normal_ins[3];
	};

	class Model {
	public:
		Model();
		~Model();

		void LoadObj(const char* filePath);

		std::vector<Triangle*> GetTriangles() const { return m_Triangles; }
		int GetTrianglesCount() const { return m_TriangleCount; }
		glm::vec4 GetBoundsMin() const { return m_BoundsMin; }
		glm::vec4 GetBoundsMax() const { return m_BoundsMax; }
	private:
		void MakeTriangles();
		void CalculateBoundingBox();
		void CleanTrash();
	private:
		int m_TriangleCount = 0;
		std::vector<glm::vec3*> m_Vertices;
		std::vector<glm::vec3*> m_Normals;
		std::vector<Face*> m_Faces;
		std::vector<Triangle*> m_Triangles;
		glm::vec4 m_BoundsMin;
		glm::vec4 m_BoundsMax;
		glm::vec3 Position{0.0f};
	};
}
#endif // !MODEL_H

