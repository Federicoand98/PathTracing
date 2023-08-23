#include "Image.h"

Image::Image(std::string_view path) : m_Width(0), m_Height(0), m_Texture(0) {
}

Image::Image(uint32_t width, uint32_t height, const void* data /*= nullptr*/) : m_Width(width), m_Height(height), m_Texture(0) {
	glGenTextures(1, &m_Texture);
	glBindTexture(GL_TEXTURE_2D, m_Texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if(data)
		SetData(data);
}

Image::~Image() {
    Release();
}

void Image::SetData(const void* data) {
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

void Image::Release() {
    glDeleteTextures(1, &m_Texture);
    m_Texture = 0;
}

void Image::Resize(uint32_t width, uint32_t height, const void* data) {
    if(m_Texture && m_Width == width && m_Height == height)
        return;

    m_Width = width;
    m_Height = height;

    if(data)
        SetData(data);
}
