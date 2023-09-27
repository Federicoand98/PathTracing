#pragma once

#ifndef IMAGE_H
#define IMAGE_H

#include <iostream>
#include <string>
#include <vector>
#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include "../stb_image.h"

namespace PathTracer {

	class Texture {
	public:
		Texture(std::string_view path);
		Texture(int target);
		~Texture();

		void MutableAllocate(uint32_t width, uint32_t height);
		void ImmutableAllocate(uint32_t width, uint32_t height, int levels = 1);
		void SubTexture3D(int width, int height, int depth, const void* data, int level = 0, int xOffset = 0, int yOffset = 0, int zOffset = 0);
		void SetData(const void* data);
		void* GetData() const {};
		void Resize(uint32_t width, uint32_t height, const void* data = nullptr);

		void LoadCubeMap(const std::vector<std::string>& paths);

		void AttachImage(int unit, int level);
		void AttachSampler(int unit);
		void Bind();
		void Unbind();

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }
		GLuint GetTexture() const { return m_Texture; }
	private:
		void Release();
	private:
		uint32_t m_Width = 0, m_Height = 0;
		int m_Target;
		std::string m_FilePath;
		GLuint m_Texture;
	};
}

#endif // IMAGE_H