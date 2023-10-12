//
// Created by feder on 23/09/2023.
//

#ifndef PATHTRACING_FRAMEBUFFER_H
#define PATHTRACING_FRAMEBUFFER_H

#include "ptpch.h"

namespace PathTracer {

	class FrameBuffer {
	public:
		FrameBuffer(int width, int height);
		~FrameBuffer();

		void Bind() const;
		void Unbind() const;

		void Rescale(int width, int height);

		void AttachTexture(unsigned int texture);

		void SavePPMTexture();

		unsigned int GetTexture() const { return m_Texture; }
		int GetWidth() const { return m_Width; }
		int GetHeight() const { return m_Height; }

	private:
		unsigned int m_FBO;
		unsigned int m_RBO;
		unsigned int m_Texture;
		int m_Width, m_Height;
		int screenshotsCount = 0;
	};
}

#endif //PATHTRACING_FRAMEBUFFER_H
