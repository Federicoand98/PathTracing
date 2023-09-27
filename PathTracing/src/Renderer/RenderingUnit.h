#pragma once

namespace PathTracer {

	class RenderingUnit {
	public:
		virtual void Begin() = 0;
		virtual void End() = 0;
	};
}
