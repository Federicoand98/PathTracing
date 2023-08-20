#include "Image.h"

Image::Image(std::string_view path) {

}

Image::Image(uint32_t width, uint32_t height, const void* data /*= nullptr*/) : m_Width(width), m_Height(height) {
	glGenTextures(1, &m_Texture);
	glBindTexture(GL_TEXTURE_2D, m_Texture);

	if(data)
		SetData(data);
}

Image::~Image() {

}

void Image::Draw() {

}

void Image::SetData(const void* data) {

}

void Image::Resize(uint32_t width, uint32_t height) {

}
