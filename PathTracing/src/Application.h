#pragma once

#ifndef APPLICATION_H
#define APPLICATION_H

#include <iostream>
#include <GL/glew.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include "Graphics/Texture.h"
#include "Renderer/Renderer.h"
#include "Graphics/FrameBuffer.h"
#include "ImGuizmo.h"
#include "ptpch.h"

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

	// I valori 0/1/2 combaciano con ObjectType dello shader (pickType).
	// Box non esiste nello shader: e' un gruppo di 6 quad, mappato lato CPU.
	enum class SelectionType {
		None = -1,
		Sphere = 0,
		Quad = 1,
		Mesh = 2,
		Box = 3
	};

	struct Selection {
		SelectionType Type = SelectionType::None;
		int Index = -1;

		bool IsValid() const { return Type != SelectionType::None && Index >= 0; }
		// una sfera non ha orientamento, un box e' axis-aligned: niente rotazione
		bool SupportsRotation() const { return Type == SelectionType::Quad || Type == SelectionType::Mesh; }
	};

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

		// selezione + gizmo
		void UpdateSelection();
		void DrawGizmo();
		void SetSelectionFromPick(int objectType, int objectIndex);
		void ValidateSelection();
		void DrawSelectionOverlay();
		ImGuizmo::OPERATION EffectiveGizmoOperation() const;
		glm::vec3 GetSelectionPivot() const;
		void GetSelectionBounds(glm::vec3& outMin, glm::vec3& outMax) const;
		std::string GetSelectionLabel() const;
		void ApplyDelta(const glm::mat4& delta);
	private:
		Selection m_Selection;
		ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::TRANSLATE;
		glm::mat4 m_GizmoMatrix{ 1.0f };

		// Rettangolo a schermo dell'immagine path-traced dentro la finestra "Viewport".
		// La 3D view non e' il background: e' una AddImage in un pannello ImGui, quindi
		// picking, gizmo e overlay devono essere ancorati a QUESTO rettangolo.
		ImVec2 m_ViewportPos{ 0.0f, 0.0f };
		ImVec2 m_ViewportSize{ 0.0f, 0.0f };
		bool m_ViewportHovered = false;

		// diagnostica del picking (visibile nel pannello)
		glm::ivec2 m_LastPickPixel{ -1, -1 };
		int m_LastPickType = -1;
		int m_LastPickIndex = -1;
		int m_PickRequestCount = 0;

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
