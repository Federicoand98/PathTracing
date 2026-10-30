#pragma once

#ifndef GPUTIMER_H
#define GPUTIMER_H

#include <GL/glew.h>

namespace PathTracer {

	// Tempo GPU di una passata, misurato con GL_TIME_ELAPSED. I risultati arrivano con qualche
	// frame di ritardo: un piccolo anello di query evita di bloccare la CPU ad aspettarli.
	// Le query GL_TIME_ELAPSED non si annidano: le passate misurate devono essere in sequenza.
	class GpuTimer {
	public:
		~GpuTimer() {
			if (m_Queries[0]) glDeleteQueries(COUNT, m_Queries);
		}

		void Begin() {
			if (!m_Queries[0]) glGenQueries(COUNT, m_Queries);
			glBeginQuery(GL_TIME_ELAPSED, m_Queries[m_Index]);
		}

		void End() {
			glEndQuery(GL_TIME_ELAPSED);
			m_Index = (m_Index + 1) % COUNT;
			if (m_Started < COUNT) {
				m_Started++;
				return;
			}

			// m_Index ora punta alla query piu' vecchia, la prossima a essere riusata
			GLint available = 0;
			glGetQueryObjectiv(m_Queries[m_Index], GL_QUERY_RESULT_AVAILABLE, &available);
			if (available) {
				GLuint64 ns = 0;
				glGetQueryObjectui64v(m_Queries[m_Index], GL_QUERY_RESULT, &ns);
				m_Ms = static_cast<float>(ns) * 1e-6f;
			}
		}

		float Ms() const { return m_Ms; }

	private:
		static constexpr int COUNT = 4;
		GLuint m_Queries[COUNT] = {};
		int m_Index = 0;
		int m_Started = 0;
		float m_Ms = 0.0f;
	};
}

#endif // GPUTIMER_H
