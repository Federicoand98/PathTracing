#include "Texture.h"

namespace PathTracer {

	Texture::Texture(std::string_view path) : m_Width(0), m_Height(0), m_Texture(0) {
	}

	Texture::Texture(uint32_t width, uint32_t height, const void* data /*= nullptr*/) : m_Width(width), m_Height(height), m_Texture(0) {
		glGenTextures(1, &m_Texture);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_Texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_Width, m_Height, 0, GL_RGBA, GL_FLOAT, NULL);
	}

	Texture::~Texture() {
		Release();
	}

	void Texture::SetData(const void* data) {
	#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	#endif
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	}

	void Texture::Release() {
		glDeleteTextures(1, &m_Texture);
		m_Texture = 0;
	}

	void Texture::Resize(uint32_t width, uint32_t height, const void* data) {
		if(m_Texture && m_Width == width && m_Height == height)
			return;

		m_Width = width;
		m_Height = height;

		Release();

		glGenTextures(1, &m_Texture);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_Texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_Width, m_Height, 0, GL_RGBA, GL_FLOAT, NULL);
		/*
		glBindImageTexture(0, m_Texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_Texture);
		 */
	}

	void Texture::Bind() {
		glBindImageTexture(0, m_Texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
		glBindTextureUnit(0, m_Texture);
		//glActiveTexture(GL_TEXTURE0);
		//glBindTexture(GL_TEXTURE_2D, m_Texture);
	}

	void Texture::Unbind() {
		glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

}
