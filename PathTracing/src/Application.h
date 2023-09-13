#pragma once

#ifndef APPLICATION_H
#define APPLICATION_H

#include <iostream>
#include <GL/glew.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <algorithm>
#include <memory>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include "Image.h"
#include "Renderer.h"

class Application {
public:
	Application();
    ~Application();

    static Application& Get();

	bool Initialize(int width, int height);
	void RunLoop();
	void Shutdown();

    GLFWwindow* GetWindow() const { return m_Window; }
private:
	void CalculateTime();
	void RenderUI(float deltaTime);
	void Render(float deltaTime);
private:
    Renderer m_Renderer;
    Camera m_Camera;
	World m_World;
	bool m_IsRunning;
	bool m_Vsync = true;
	bool m_LinkColors = true;
	float m_Timer = 0.0f;
	float m_DeltaTime = 0.0f;
	float m_ResetTimer = 0.0f;
	float m_LastFrameTime = 0.0f;
	int m_NFrames = 0;
	double m_FPS = 0.0f;
	int m_Width;
	int m_Height;
    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;
	GLFWwindow* m_Window;
};

#endif // APPLICATION_H
