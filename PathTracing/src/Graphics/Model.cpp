#include "Model.h"

namespace PathTracer {

	Model::Model() {
	}

	Model::~Model() {
		for (auto t : m_Triangles) {
			delete t;
		}
		m_Triangles.clear();
	}

	void Model::LoadObj(const char* filePath) {
		std::string line, prefix;
		std::stringstream ss;
		std::ifstream ifile(filePath);

		std::cout << "Loading model: " << filePath << std::endl;

		if (!std::filesystem::exists(filePath)) {
			std::cerr << "File does not exist: " << filePath << std::endl;
			return;
		}

		if(!ifile.is_open()) {
			std::cout << "Error while opening obj file..." << std::endl;
			return;
		}

		while (std::getline(ifile, line)) {
			ss.clear();
			ss.str(line);
			ss >> prefix;

			if (prefix == "v") {
				glm::vec3* v = new glm::vec3;
				ss >> v->x >> v->y >> v->z;
				m_Vertices.push_back(v);
			}
			else if (prefix == "vn") {
				glm::vec3* vn = new glm::vec3;
				ss >> vn->x >> vn->y >> vn->z;
				m_Normals.push_back(vn);
			}
			else if (prefix == "f") {
				Face* face = new Face;
				int counter = 0, face_index_counter = 0, temp_int = 0;

				while (ss >> temp_int) {
					if (counter % 2 == 0)
						face->vertex_ins[face_index_counter] = temp_int;
					else
						face->normal_ins[face_index_counter] = temp_int;

					if (ss.peek() == '/') {
						ss.ignore(1, '/');

						if (ss.peek() == '/') {
							ss.ignore(1, '/');
							counter++;
						}
					}
					else if (ss.peek() == ' ') {
						counter++;
						ss.ignore(1, ' ');
					}

					if (counter % 2 == 0) {
						counter = 0;
						face_index_counter++;
					}
				}

				m_Faces.push_back(face);
				m_TriangleCount++;
			}
		}
		ifile.close();

		MakeTriangles();
		CalculateBoundingBox();
		CleanTrash();
	}

	void Model::MakeTriangles() {
		// Smooth shading (come "Shade Smooth" di Blender): invece di usare le normali
		// per-faccia dell'OBJ (che danno lo shading sfaccettato), calcolo una normale
		// per-vertice mediando le normali geometriche di tutte le facce adiacenti.
		// Il cross product NON normalizzato pesa ogni contributo per l'area del triangolo.
		std::vector<glm::vec3> vertexNormals(m_Vertices.size(), glm::vec3(0.0f));

		for (auto& face : m_Faces) {
			const glm::vec3& a = *m_Vertices[face->vertex_ins[0] - 1];
			const glm::vec3& b = *m_Vertices[face->vertex_ins[1] - 1];
			const glm::vec3& c = *m_Vertices[face->vertex_ins[2] - 1];

			glm::vec3 faceNormal = glm::cross(b - a, c - a); // peso implicito per area

			vertexNormals[face->vertex_ins[0] - 1] += faceNormal;
			vertexNormals[face->vertex_ins[1] - 1] += faceNormal;
			vertexNormals[face->vertex_ins[2] - 1] += faceNormal;
		}

		for (glm::vec3& n : vertexNormals) {
			if (glm::length(n) > 1e-8f)
				n = glm::normalize(n);
		}

		for (auto& face : m_Faces) {
			Triangle* triangle = new Triangle;

			triangle->A = glm::vec4(*(m_Vertices[face->vertex_ins[0] - 1]), 0.0f);
			triangle->B = glm::vec4(*(m_Vertices[face->vertex_ins[1] - 1]), 0.0f);
			triangle->C = glm::vec4(*(m_Vertices[face->vertex_ins[2] - 1]), 0.0f);

			triangle->NormalA = glm::vec4(vertexNormals[face->vertex_ins[0] - 1], 0.0f);
			triangle->NormalB = glm::vec4(vertexNormals[face->vertex_ins[1] - 1], 0.0f);
			triangle->NormalC = glm::vec4(vertexNormals[face->vertex_ins[2] - 1], 0.0f);

			m_Triangles.push_back(triangle);
		}
	}

	void Model::CalculateBoundingBox() {
		m_BoundsMin = glm::vec4(std::numeric_limits<float>::max());
		m_BoundsMax = glm::vec4(std::numeric_limits<float>::lowest());

		for (const Triangle* triangle : m_Triangles) {
			m_BoundsMin = glm::min(m_BoundsMin, triangle->A);
			m_BoundsMin = glm::min(m_BoundsMin, triangle->B);
			m_BoundsMin = glm::min(m_BoundsMin, triangle->C);

			m_BoundsMax = glm::max(m_BoundsMax, triangle->A);
			m_BoundsMax = glm::max(m_BoundsMax, triangle->B);
			m_BoundsMax = glm::max(m_BoundsMax, triangle->C);
		}
	}

	void Model::CleanTrash() {
		for (auto v : m_Vertices)
			delete v;

		for (auto vn : m_Normals)
			delete vn;

		for (auto f : m_Faces)
			delete f;
	}
}
