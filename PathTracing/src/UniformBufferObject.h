#pragma once

#ifndef UNIFORMBUFFEROBJECT_H
#define UNIFORMBUFFEROBJECT_H

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

class UniformBufferObject {
private:
	unsigned int m_UBO = 0;
	size_t m_Size = 0;
	size_t m_Offset = 0;
public:
	UniformBufferObject() {}

	~UniformBufferObject() {
		glDeleteBuffers(1, &m_UBO);
	}

	void Init() {
		glGenBuffers(1, &m_UBO);
		glBindBuffer(GL_UNIFORM_BUFFER, m_UBO);
	}

	void Bind(unsigned int bindingPoint) {
		glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, m_UBO);
	}

	void Unbind(unsigned int bindingPoint) {
		glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, 0);
	}

	void PrepareSize(size_t size) {
		m_Size = size;
		m_Offset = 0;
		glBufferData(GL_UNIFORM_BUFFER, m_Size, nullptr, GL_STATIC_DRAW);
	}

	template<typename T>
	void SubData(const T& data) {
		glBufferSubData(GL_UNIFORM_BUFFER, m_Offset, sizeof(T), &data);
		m_Offset += sizeof(T);
	}

	template<>
	void SubData<float>(const float& data) {
		glBufferSubData(GL_UNIFORM_BUFFER, m_Offset, sizeof(float), &data);
		m_Offset += sizeof(float);
	}

	template<>
	void SubData<glm::vec3>(const glm::vec3& data) {
		glBufferSubData(GL_UNIFORM_BUFFER, m_Offset, sizeof(glm::vec3), &data);
		m_Offset += sizeof(glm::vec3);
	}

	template<typename T>
	void SubDataList(const std::vector<T>& data) {
		glBufferSubData(GL_UNIFORM_BUFFER, m_Offset, sizeof(T) * data.size(), &data[0]);
		m_Offset += sizeof(T) * data.size();
	}
};

#endif
