#pragma once

#ifndef Application_h__
#define Application_h__

#include <iostream>
#include <GL/glew.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Image.h"
#include "Renderer.h"
#include <stdio.h>
#include <algorithm>
#include <memory>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

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
	void RenderUI(float deltaTime);
	void Render();
private:
    Renderer m_Renderer;
	bool m_IsRunning;
	float m_LastFrame;
	int m_Width;
	int m_Height;
    uint32_t m_ViewportWidth = 0;
    uint32_t m_ViewportHeight = 0;
	GLFWwindow* m_Window;

//    std::shared_ptr<Image> m_FinalImage;
//    unsigned char* m_ImageData = nullptr;
};

#endif // Application_h__
