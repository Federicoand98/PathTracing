#pragma once

#ifndef Image_h__
#define Image_h__

#include <string>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

class Image {
public:
	Image(std::string_view path);
	Image(uint32_t width, uint32_t height, const void* data = nullptr);
	~Image();

	void Draw();
	void SetData(const void* data);
	void Resize(uint32_t width, uint32_t height);

	uint32_t GetWidth() const { return m_Width; }
	uint32_t GetHeight() const { return m_Height; }
private:
	uint32_t m_Width = 0, m_Height = 0;
	std::string m_FilePath;
	GLuint m_Texture;
};

#endif // Image_h__