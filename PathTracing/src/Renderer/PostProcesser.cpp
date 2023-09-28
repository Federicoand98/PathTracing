#include "PostProcesser.h"

#include "../Graphics/FrameBuffer.h"

namespace PathTracer {

	PostProcesser::PostProcesser(const class FrameBuffer& frameBuffer) {
		m_FrameBuffer = &frameBuffer;
		m_Shader = std::make_shared<Shader>("shaders/vScreenQuad.vert", "shaders/fScreenQuad.frag");
	}

	PostProcesser::~PostProcesser() {
		m_Shader.reset();
	}

	void PostProcesser::Begin()	{
		m_FrameBuffer->Bind();
		m_Shader->Bind();
	}

	void PostProcesser::End() {
		m_FrameBuffer->Unbind();
		m_Shader->Unbind();
	}

	void PostProcesser::UploadUniforms(bool enable, float exposure) {
		m_Shader->setInt("tex", 0);
		m_Shader->setBool("PostProcessing", enable);
		m_Shader->setFloat("Exposure", exposure);
	}
}