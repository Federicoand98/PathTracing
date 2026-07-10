#include "Model.h"

namespace PathTracer {

	namespace {
		// Un token di faccia OBJ puo' essere "v", "v/vt", "v//vn" o "v/vt/vn".
		// Estrae indice di vertice e (se presente) di normale, ignorando le UV.
		// Ritorna false se il token non contiene un indice di vertice valido.
		bool ParseFaceToken(const std::string& token, int& vertexIndex, int& normalIndex) {
			vertexIndex = 0;
			normalIndex = 0;

			auto toInt = [](const std::string& s, int& out) -> bool {
				if (s.empty()) return false;
				try { out = std::stoi(s); }
				catch (...) { return false; }
				return true;
			};

			size_t firstSlash = token.find('/');
			if (firstSlash == std::string::npos)
				return toInt(token, vertexIndex);

			if (!toInt(token.substr(0, firstSlash), vertexIndex))
				return false;

			size_t secondSlash = token.find('/', firstSlash + 1);
			if (secondSlash != std::string::npos)
				toInt(token.substr(secondSlash + 1), normalIndex); // opzionale

			return true;
		}
	}

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

		m_Name = std::filesystem::path(filePath).filename().string();

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
				// Le facce OBJ possono essere poligoni (quad, ngon), non solo triangoli:
				// le triangoliamo a ventaglio attorno al primo vertice.
				std::vector<int> faceVertices, faceNormals;
				std::string token;

				while (ss >> token) {
					int v = 0, vn = 0;
					if (!ParseFaceToken(token, v, vn))
						continue;

					// gli indici negativi sono relativi alla fine della lista corrente
					if (v < 0) v = static_cast<int>(m_Vertices.size()) + v + 1;
					if (vn < 0) vn = static_cast<int>(m_Normals.size()) + vn + 1;

					faceVertices.push_back(v);
					faceNormals.push_back(vn);
				}

				if (faceVertices.size() < 3)
					continue;

				for (size_t i = 1; i + 1 < faceVertices.size(); i++) {
					Face* face = new Face;

					face->vertex_ins[0] = faceVertices[0];
					face->vertex_ins[1] = faceVertices[i];
					face->vertex_ins[2] = faceVertices[i + 1];

					face->normal_ins[0] = faceNormals[0];
					face->normal_ins[1] = faceNormals[i];
					face->normal_ins[2] = faceNormals[i + 1];

					m_Faces.push_back(face);
					m_TriangleCount++;
				}
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

		// scarta le facce che referenziano vertici fuori range (OBJ malformato)
		auto validFace = [this](const Face* f) {
			for (int i = 0; i < 3; i++) {
				int idx = f->vertex_ins[i] - 1;
				if (idx < 0 || idx >= static_cast<int>(m_Vertices.size()))
					return false;
			}
			return true;
		};

		size_t skipped = 0;
		for (auto& face : m_Faces) {
			if (!validFace(face)) {
				skipped++;
				continue;
			}

			const glm::vec3& a = *m_Vertices[face->vertex_ins[0] - 1];
			const glm::vec3& b = *m_Vertices[face->vertex_ins[1] - 1];
			const glm::vec3& c = *m_Vertices[face->vertex_ins[2] - 1];

			glm::vec3 faceNormal = glm::cross(b - a, c - a); // peso implicito per area

			vertexNormals[face->vertex_ins[0] - 1] += faceNormal;
			vertexNormals[face->vertex_ins[1] - 1] += faceNormal;
			vertexNormals[face->vertex_ins[2] - 1] += faceNormal;
		}

		if (skipped > 0)
			std::cerr << "Warning: skipped " << skipped << " faces with out-of-range vertex indices" << std::endl;

		for (glm::vec3& n : vertexNormals) {
			if (glm::length(n) > 1e-8f)
				n = glm::normalize(n);
		}

		for (auto& face : m_Faces) {
			if (!validFace(face))
				continue;

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
