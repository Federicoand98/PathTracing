#pragma once

#ifndef IMAGE_H
#define IMAGE_H

#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

namespace PathTracer {

	class Texture {
	public:
		Texture(std::string_view path);
		Texture(uint32_t width, uint32_t height, const void* data = nullptr);
		~Texture();

		void SetData(const void* data);
		void* GetData() const {};
		void Resize(uint32_t width, uint32_t height, const void* data = nullptr);

		void Bind();
		void Unbind();

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }
		GLuint GetTexture() const { return m_Texture; }
	private:
		void Release();
	private:
		uint32_t m_Width = 0, m_Height = 0;
		std::string m_FilePath;
		GLuint m_Texture;
	};
}

#endif // IMAGE_H