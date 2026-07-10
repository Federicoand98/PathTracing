#pragma once

#ifndef COMPUTESHADER_H
#define COMPUTESHADER_H

#include <GL/glew.h>
#include "ptpch.h"
#include "../World.h"

namespace PathTracer {

	class ComputeShader {
	public:
		unsigned int ID;
		unsigned int ssbo_s;
		unsigned int ssbo_m;
		unsigned int ssbo_q;
		unsigned int ssbo_t;
		unsigned int ssbo_tn;
		unsigned int ssbo_tuv;
		unsigned int ssbo_mesh;
		unsigned int ssbo_cubes;
		unsigned int ssbo_idx;
		unsigned int ssbo_bvh;
		unsigned int ssbo_pick;

		ComputeShader(const char* path) {
			std::string computeCode;
			std::ifstream computeFile;

			computeFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

			try {
				computeFile.open(path);
				std::stringstream computeStream;
				computeStream << computeFile.rdbuf();
				computeFile.close();
				computeCode = computeStream.str();
			}
			catch (std::ifstream::failure& e) {
				std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
			}

			const char* shaderCode = computeCode.c_str();
			unsigned int compute;

			// compute shader
			compute = glCreateShader(GL_COMPUTE_SHADER);
			glShaderSource(compute, 1, &shaderCode, NULL);
			glCompileShader(compute);
			checkCompileErrors(compute, "COMPUTE");

			// shader Program
			ID = glCreateProgram();
			glAttachShader(ID, compute);
			glLinkProgram(ID);
			checkCompileErrors(ID, "PROGRAM");

			glDeleteShader(compute);

			glGenBuffers(1, &ssbo_s);
			glGenBuffers(1, &ssbo_m);
			glGenBuffers(1, &ssbo_q);
			glGenBuffers(1, &ssbo_t);
			glGenBuffers(1, &ssbo_tn);
			glGenBuffers(1, &ssbo_tuv);
			glGenBuffers(1, &ssbo_mesh);
			glGenBuffers(1, &ssbo_cubes);
			glGenBuffers(1, &ssbo_idx);
			glGenBuffers(1, &ssbo_bvh);
			glGenBuffers(1, &ssbo_pick);

			ResetPickBuffer();
		}

		// Azzera il risultato prima del dispatch. La sentinella e' -2, non -1: cosi'
		// "lo shader non ha scritto" (-2) si distingue da "raggio a vuoto" (-1).
		void ResetPickBuffer() {
			const int empty[2] = { -2, -2 };
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_pick);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(empty), empty, GL_DYNAMIC_DRAW);
		}

		void ReadPickBuffer(int& objectType, int& objectIndex) {
			int result[2] = { -1, -1 };
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_pick);
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(result), result);

			objectType = result[0];
			objectIndex = result[1];
		}
		
		void Bind() {
			glUseProgram(ID);
		}

		void Unbind() {
			glUseProgram(0);
		}

		void SetBool(const std::string& name, bool value) const {
			glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
		}

		void SetInt(const std::string& name, int value) const {
			glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
		}

		void SetIVec2(const std::string& name, int x, int y) const {
			glUniform2i(glGetUniformLocation(ID, name.c_str()), x, y);
		}

		void SetFloat(const std::string& name, float value) const {
			glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
		}

		void SetVec3(const std::string& name, const glm::vec3& vec) const {
			glUniform3f(glGetUniformLocation(ID, name.c_str()), vec.x, vec.y, vec.z);
		}

		void SetMat4(const std::string& name, const glm::mat4& mat) {
			glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
		}

		void SetWorld() {
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo_s);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo_m);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo_q);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, ssbo_t);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssbo_mesh);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, ssbo_cubes);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, ssbo_bvh);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, ssbo_idx);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, ssbo_tn);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, ssbo_pick);
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, ssbo_tuv);
		}

		void UpdateWorldBuffer(const World& world, bool fullReset = false) {
			// Upload incondizionato di tutti i buffer della scena: un vettore vuoto
			// azzera il buffer GPU corrispondente. Necessario perche' al cambio scena
			// una collezione che si svuota (es. niente sfere nella Cornell Box con mesh)
			// altrimenti lascerebbe elementi "fantasma" della scena precedente.
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_s);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Sphere) * world.Spheres.size(), world.Spheres.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_m);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Material) * world.Materials.size(), world.Materials.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_q);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Quad) * world.Quads.size(), world.Quads.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_t);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(TrianglePosition) * world.TriPositions.size(), world.TriPositions.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_tn);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(TriangleNormal) * world.TriNormals.size(), world.TriNormals.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_tuv);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(TriangleUV) * world.TriUVs.size(), world.TriUVs.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_mesh);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(MeshInfo) * world.Meshes.size(), world.Meshes.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_cubes);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Box) * world.Boxes.size(), world.Boxes.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_bvh);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(BVHNodeNew) * world.BVHNodes.size(), world.BVHNodes.data(), GL_STATIC_DRAW);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_idx);
			glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(int) * world.TriIndex.size(), world.TriIndex.data(), GL_STATIC_DRAW);
		}

	private:
		void checkCompileErrors(unsigned int shader, std::string type) {
			int success;
			char infoLog[1024];
			if (type != "PROGRAM") {
				glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
				if (!success) {
					glGetShaderInfoLog(shader, 1024, NULL, infoLog);
					std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
				}
			}
			else {
				glGetProgramiv(shader, GL_LINK_STATUS, &success);
				if (!success) {
					glGetProgramInfoLog(shader, 1024, NULL, infoLog);
					std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
				}
			}
		}
	};

}


#endif // !COMPUTESHADER_H

