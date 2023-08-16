#pragma once

#ifndef Application_h__
#define Application_h__

#include <iostream>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

class Application {
public:
	Application();

	bool Initialize(int width, int height);
	void RunLoop();
	void Shutdown();
private:
	void Render(float deltaTime);
private:
	bool m_IsRunning;
	float m_LastFrame;
	int m_Width;
	int m_Height;
	GLFWwindow* m_Window;
	ImGuiIO io;
};

#endif // Application_h__
