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
#include "Graphics/Texture.h"
#include "Renderer/Renderer.h"
#include "Graphics/FrameBuffer.h"

/*
* @Class Application
* @brief The Application class is the main entry poiny for the application.
* 
* This class is responsible for initializating, running, and shutting down the application.
* 
* @member m_Renderer: An instance of the Renderer ckass that handles all rendering tasks.
* @member m_Camera: An instace of the Camera class that represents the camera in the 3D world.
* @member m_World: An instance of the World class that represents the 3D world.
*/

namespace PathTracer {

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
		//FrameBuffer* m_Framebuffer;
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
}

#endif // APPLICATION_H
