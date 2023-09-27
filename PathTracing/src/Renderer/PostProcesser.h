#pragma once

#include "RenderingUnit.h"
#include "../Graphics/Shader.h"

namespace PathTracer {

	class PostProcesser : public RenderingUnit {
	public:
		PostProcesser(const class FrameBuffer& frameBuffer);
		~PostProcesser();

		void Begin() override;
		void End() override;

		void UploadUniforms(bool enable);
	private:
		std::shared_ptr<Shader> m_Shader;
		const class FrameBuffer* m_FrameBuffer = nullptr;
	};
}
