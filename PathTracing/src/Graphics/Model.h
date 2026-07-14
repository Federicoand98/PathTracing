#pragma once

#ifndef MODEL_H
#define MODEL_H

#include "ptpch.h"
#include "../Renderer/Primitives.h"

namespace PathTracer {

	class Model {
	public:
		Model();
		~Model();

		void LoadObj(const char* filePath);

		std::vector<Triangle*> GetTriangles() const { return m_Triangles; }
		int GetTrianglesCount() const { return m_TriangleCount; }
		const std::string& GetName() const { return m_Name; }
		const std::string& GetPath() const { return m_Path; } // path completo, per la serializzazione
		glm::vec4 GetBoundsMin() const { return m_BoundsMin; }
		glm::vec4 GetBoundsMax() const { return m_BoundsMax; }
	private:
		void MakeTriangles();
		void CalculateBoundingBox();
		void CleanTrash();
	private:
		int m_TriangleCount = 0;
		std::string m_Name;
		std::string m_Path;
		std::vector<glm::vec3*> m_Vertices;
		std::vector<glm::vec2> m_UVs;
		std::vector<glm::vec3*> m_Normals;
		std::vector<Face*> m_Faces;
		std::vector<Triangle*> m_Triangles;
		glm::vec4 m_BoundsMin;
		glm::vec4 m_BoundsMax;
		glm::vec3 Position{0.0f};
	};
}
#endif // !MODEL_H

