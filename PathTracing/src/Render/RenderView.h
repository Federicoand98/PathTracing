#pragma once

#ifndef RENDERVIEW_H
#define RENDERVIEW_H

#include "ptpch.h"

namespace Lucerna {

	// Da dove si guarda, per il renderer: solo matrici e posizione. Chi naviga (la camera
	// della vista, nell'Editor) la costruisce a ogni frame; il renderer non sa niente di input.
	struct RenderView {
		glm::mat4 Projection{ 1.0f };
		glm::mat4 View{ 1.0f };
		glm::mat4 InverseProjection{ 1.0f };
		glm::mat4 InverseView{ 1.0f };
		glm::vec3 Position{ 0.0f };
	};
}

#endif // RENDERVIEW_H
