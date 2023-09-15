#include "Model.h"

Model::Model() {
}

Model::~Model() {
	for (auto t : m_Triangles) {
		delete t;
	}
}

void Model::LoadObj(const char* filePath) {
	std::string line, prefix;
	std::stringstream ss;
	std::ifstream ifile(filePath);

	if (ifile.is_open()) {
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
	else {
		std::cout << "Error while opening file..." << std::endl;
	}
}

void Model::MakeTriangles() {
	for (auto& face : m_Faces) {
		Triangle* triangle = new Triangle;

		triangle->A = glm::vec4(*(m_Vertices[face->vertex_ins[0] - 1]), 0.0f);
		triangle->B = glm::vec4(*(m_Vertices[face->vertex_ins[1] - 1]), 0.0f);
		triangle->C = glm::vec4(*(m_Vertices[face->vertex_ins[2] - 1]), 0.0f);

		triangle->NormalA = glm::vec4(*(m_Normals[face->normal_ins[0] - 1]), 0.0f);
		triangle->NormalB = glm::vec4(*(m_Normals[face->normal_ins[1] - 1]), 0.0f);
		triangle->NormalC = glm::vec4(*(m_Normals[face->normal_ins[2] - 1]), 0.0f);

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

	std::cout << "Min: " << glm::to_string(m_BoundsMin) << std::endl;
	std::cout << "Min: " << glm::to_string(m_BoundsMax) << std::endl;
}

void Model::CleanTrash() {
	for (auto v : m_Vertices)
		delete v;

	for (auto vn : m_Normals)
		delete vn;

	for (auto f : m_Faces)
		delete f;
}
