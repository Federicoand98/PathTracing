#include "Texture.h"

namespace PathTracer {

	Texture::Texture(std::string_view path) : m_Width(0), m_Height(0), m_Texture(0) {
	}

	Texture::Texture(int target) : m_Target(target), m_Texture(0) {
		glCreateTextures(target, 1, &m_Texture);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(m_Target, m_Texture);
		glTexParameteri(m_Target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(m_Target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(m_Target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(m_Target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	Texture::~Texture() {
		Release();
	}

	void Texture::MutableAllocate(uint32_t width, uint32_t height) {
		m_Width = width;
		m_Height = height;

		Bind();

		switch (m_Target) {
			case GL_TEXTURE_2D:
				glTexImage2D(m_Target, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
				break;

			case GL_TEXTURE_CUBE_MAP:
				for (int i = 0; i < 6; i++) {
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
				}
				break;

			default:
				return;
		}
	}

	void Texture::ImmutableAllocate(uint32_t width, uint32_t height, int levels) {
		m_Width = width;
		m_Height = height;

		glTextureStorage2D(m_Texture, levels, GL_RGBA32F, width, height);
	}

	void Texture::SubTexture3D(int width, int height, int depth, const void* data, int level, int xOffset, int yOffset, int zOffset) {
		glTexSubImage3D(m_Texture, level, xOffset, yOffset, zOffset, width, height, depth, GL_RGBA, GL_UNSIGNED_BYTE, data);
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

	void Texture::LoadCubeMap(const std::vector<std::string>& paths) {
		if (paths.size() != 6) {
			std::cout << "Texture: Failed LoadCubeMap!" << std::endl;
			return;
		}

		std::vector<unsigned char*> images;
		// non li memorizzo tutti perche le cubemap sono sempre immagini quadrate
		int width, height, nChannels;

		// TODO: Parallelo
		for (int i = 0; i < 6; i++) {
			unsigned char* data = stbi_load(paths.at(i).c_str(), &width, &height, &nChannels, 0);
			images.push_back(data);
		}

		ImmutableAllocate(width, height, 1);

		for (int i = 0; i < 6; i++) {
			SubTexture3D(width, height, 1, images.at(i), 0, 0, 0, i);
			delete[] images.at(i);
		}

		images.clear();
	}

	void Texture::AttachImage(int unit, int level) {
		glBindImageTexture(unit, m_Texture, level, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
	}

	void Texture::AttachSampler(int unit) {
		glBindTextureUnit(unit, m_Texture);
	}

	void Texture::Bind() {
		//glActiveTexture(GL_TEXTURE0);
		glBindTexture(m_Target, m_Texture);
	}

	void Texture::Unbind() {
		glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
		glBindTexture(m_Target, 0);
	}

}
