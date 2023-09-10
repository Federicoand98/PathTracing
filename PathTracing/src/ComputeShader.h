#pragma once

#ifndef COMPUTESHADER_H
#define COMPUTESHADER_H

#include <GL/glew.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include "World.h"

class ComputeShader {
public:
	unsigned int ID;
    unsigned int ssbo_s;
    unsigned int ssbo_m;

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
	}
	
    void Use() {
        glUseProgram(ID);
    }

    void SetBool(const std::string& name, bool value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }

    void SetInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
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
    }

    void UpdateWorldBuffer(const World& world) {
        glGenBuffers(1, &ssbo_s);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_s);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Sphere2) * world.Spheres.size(), world.Spheres.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &ssbo_m);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo_m);
        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(Material) * world.Materials.size(), world.Materials.data(), GL_STATIC_DRAW);
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

#endif // !COMPUTESHADER_H

