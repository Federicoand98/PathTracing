#pragma once

#ifndef MODEL_H
#define MODEL_H

#include "ptpch.h"
#include "../Renderer/Primitives.h"

namespace PathTracer {

	// Descrittore di un materiale letto dal .mtl. Contiene SOLO i campi che il nostro
	// Material sa rappresentare; il resto del MTL (Ka, map_Bump, ...) viene ignorato.
	// World lo traduce in un Material dell'engine e registra la texture.
	struct MtlMaterial {
		std::string Name;
		glm::vec3 Kd{ 0.8f };   // diffuse color
		glm::vec3 Ke{ 0.0f };   // emissive color
		float Ns = 32.0f;       // shininess -> roughness
		float Ni = 1.0f;        // densita' ottica -> IOR
		float d = 1.0f;         // opacita' (1 = opaco); niente cutout per ora
		float Pr = -1.0f;       // roughness PBR (-1 = assente)
		float Pm = -1.0f;       // metallic PBR  (-1 = assente)
		std::string map_Kd;     // path texture diffuse gia' risolto, vuoto = nessuna
	};

	class Model {
	public:
		Model();
		~Model();

		void LoadObj(const char* filePath);

		std::vector<Triangle*> GetTriangles() const { return m_Triangles; }
		int GetTrianglesCount() const { return m_TriangleCount; }
		const std::vector<MtlMaterial>& GetMaterials() const { return m_Materials; } // vuoto = OBJ senza MTL
		const std::string& GetName() const { return m_Name; }
		const std::string& GetPath() const { return m_Path; } // path completo, per la serializzazione
		glm::vec4 GetBoundsMin() const { return m_BoundsMin; }
		glm::vec4 GetBoundsMax() const { return m_BoundsMax; }
	private:
		void MakeTriangles();
		void CalculateBoundingBox();
		void CleanTrash();
		void ParseMtl(const std::string& mtlPath); // popola m_Materials
	private:
		int m_TriangleCount = 0;
		std::string m_Name;
		std::string m_Path;
		std::vector<glm::vec3*> m_Vertices;
		std::vector<glm::vec2> m_UVs;
		std::vector<glm::vec3*> m_Normals;
		std::vector<Face*> m_Faces;
		std::vector<Triangle*> m_Triangles;
		std::vector<MtlMaterial> m_Materials;
		glm::vec4 m_BoundsMin;
		glm::vec4 m_BoundsMax;
		glm::vec3 Position{0.0f};
	};
}
#endif // !MODEL_H

