#pragma once

#ifndef APPLICATION_H
#define APPLICATION_H

#include "Editor/Preferences.h"
#include <iostream>
#include <filesystem>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <GLFW/glfw3.h>
#include "Vulkan/VulkanContext.h"
#include "Render/Renderer.h"
#include "Editor/Camera.h"
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

namespace Lucerna {

	// I valori 0/1/2 combaciano con ObjectType dello shader (pickType).
	// Box non esiste nello shader: e' un gruppo di 6 quad, mappato lato CPU.
	enum class SelectionType {
		None = -1,
		Sphere = 0,
		Quad = 1,
		Mesh = 2,
		Box = 3,
		Light = 4   // luce punto o spot: nessun raggio la colpisce, si seleziona dall'outliner
	};

	// Cosa e' selezionato: il tipo e l'id dell'oggetto (issue #23). L'indice nella sua lista si
	// ricava quando serve (Application::IndexOf): cancellare un altro oggetto non lo sposta.
	struct Selection {
		SelectionType Type = SelectionType::None;
		ObjectId Id;

		bool IsValid() const { return Type != SelectionType::None && !Id.IsNil(); }
		// una sfera non ha orientamento, un box e' axis-aligned: niente rotazione.
		// La rotazione di una luce orienta lo spot.
		bool SupportsRotation() const { return Type == SelectionType::Quad || Type == SelectionType::Mesh || Type == SelectionType::Light; }
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
		void HandleCaptures();

		// pannelli della UI, uno per tab
		void DrawSceneTab();
		void DrawMaterialsTab();
		void DrawRenderTab();
		void DrawDebugTab();

		void DrawOutliner();
		void DrawSelectionProperties();
		void DrawGizmoToolbar();
		void DrawAddObjectMenu();
		bool DrawMaterialEditor(Material& material, int index); // true = la copia e' cambiata
		void DrawMaterialEditorWindow();
		void DrawTextureManager();
		void DrawDeletePopup();
		void DeleteSelection();
		bool CanDeleteSelection() const;
		int SelectionCollectionSize(SelectionType type) const;
		bool SelectableObjectRow(const char* label, SelectionType type, int index, int materialIndex); // -1 = nessun materiale
		bool MaterialCombo(const char* label, ObjectId& material);
		bool TextureCombo(const char* label, ObjectId& texture, TextureKind kind);

		ObjectId EnsureMaterialExists();     // garantisce almeno un materiale, ne ritorna l'id
		glm::vec3 SpawnPosition() const;     // davanti alla camera
		void OpenMaterialEditor(int index);

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
		std::string GetSelectionName() const;
		void ApplyDelta(const glm::mat4& delta);

		// Apre un file (rimpiazzando la scena) e registra path+mtime per il live-link;
		// PollLiveLink controlla ogni frame se il file e' cambiato e in tal caso lo riapre.
		bool OpenFile(const std::string& path);
		void PollLiveLink();
		static std::vector<std::string> FileFilters(); // per il file dialog, dal registry degli importer
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

		// editor materiale flottante. L'indice va sempre validato contro Materials:
		// al cambio scena la lista si svuota sotto i piedi, come per la selezione.
		bool m_ShowMaterialEditor = false;
		int m_MaterialEditorIndex = -1;

		// La richiesta e' separata dallo stato "aperta" perche' OpenPopup usa l'ID stack
		// della finestra corrente: chiamandola dal bottone dentro "Settings" produrrebbe un
		// ID diverso da quello della modale, disegnata a livello top. Quindi si alza solo
		// un flag e la OpenPopup la fa sempre DrawDeletePopup.
		bool m_DeleteRequested = false;
		bool m_ShowDeletePopup = false;   // finche' e' vero, le scorciatoie sono sospese

		// DOF: "pick per fuoco". Armato dal bottone, il click successivo nel viewport
		// mette a fuoco il punto colpito invece di selezionare. m_PickForFocus ricorda,
		// mentre il pick e' in volo (risultato pronto il frame dopo), che era per il fuoco.
		bool m_FocusPickArmed = false;
		bool m_PickForFocus = false;

		// salva/carica scena
		char m_SceneFilename[128] = "scene";

		// creazione oggetti
		ObjectId m_NewObjectMaterial;
		std::vector<std::string> m_TextureFiles;
		int m_SelectedTextureFile = 0;

		// diagnostica del picking (visibile nel pannello)
		glm::ivec2 m_LastPickPixel{ -1, -1 };
		int m_LastPickType = -1;
		int m_LastPickIndex = -1;
		int m_PickRequestCount = 0;

		VulkanContext m_Vulkan;
		VkDescriptorPool m_ImGuiPool = VK_NULL_HANDLE;
		bool m_ScreenshotRequested = false;

		Renderer m_Renderer;
		Camera m_Camera;
		World m_World;

		// Live-link glTF (ADR 0003): il renderer osserva il file caricato e lo ricarica quando
		// cambia (tipicamente perche' Blender l'ha ri-esportato su save). Read-only verso Blender.
		std::string m_WatchPath;                        // il file aperto, vuoto = nessuno
		bool m_LiveLink = true;                         // riapre il file quando cambia
		std::filesystem::file_time_type m_WatchMtime{}; // ultimo timestamp visto
		bool m_IsRunning;
		Preferences m_Preferences;
		// La texture ImGui dell'immagine del renderer, e la versione dell'immagine a cui si riferisce
		VkDescriptorSet m_DisplaySet = VK_NULL_HANDLE;
		uint32_t m_DisplayVersion = 0;
		ImTextureID DisplayTexture();
		SceneCamera m_LastShot;

		// Selezione per id (issue #23)
		ObjectId IdOf(SelectionType type, int index) const;
		int IndexOf(const Selection& selection) const; // -1 se l'oggetto non c'e' piu'
		Selection Select(SelectionType type, int index) const { return { type, IdOf(type, index) }; }
		int SelectedIndex() const { return IndexOf(m_Selection); }
		std::string ObjectName(const Selection& selection) const;
		// Campo "Name": rinomina all'uscita dal campo, con il nome reso unico dalla lista
		void NameField(const ObjectId& id, const std::string& name);
		std::unordered_map<ObjectId, std::array<char, 128>> m_NameBuffers; // l'inquadratura del frame scorso: se cambia da fuori, la vista la segue
		void SyncCameras(bool viewMoved);
		bool m_Vsync = true; // effettivo: PT_NOVSYNC lo spegne per la sessione senza toccare le preferenze
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
