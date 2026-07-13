#include "stb_image_write.h"
#include "Application.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "imgui_internal.h"
#include "Input.h"
#include "Random.h"

namespace PathTracer {

	// il testo prima di ### e' il titolo, il resto e' l'ID stabile
	static const char* kDeletePopupId = "Delete object?###DeleteObject";

	static Application* s_Instance = nullptr;

	namespace {
		// Il dockspace copre l'intera viewport, quindi io.WantCaptureMouse sarebbe sempre
		// vero e il picking non scatterebbe mai. Non possiamo pero' riconoscere il
		// dockspace dal nome: quando il mouse e' su un dock node, ImGui riporta una
		// finestra host interna con nome generato ("DockSpace Demo/DockSpace_0000001A").
		// Usiamo invece i flag: il dockspace ha NoBringToFrontOnFocus, i pannelli no.
		const char* HoveredWindowName() {
			ImGuiWindow* hovered = ImGui::GetCurrentContext()->HoveredWindow;
			return hovered ? hovered->Name : "(nessuna)";
		}

		// Proietta un punto world sullo schermo. Ritorna false se sta dietro la camera:
		// dividere per una w negativa produrrebbe un punto ribaltato davanti all'occhio.
		bool WorldToScreen(const glm::mat4& viewProjection, const glm::vec3& point,
						   const ImVec2& viewportPos, const ImVec2& viewportSize, ImVec2& outScreen) {
			glm::vec4 clip = viewProjection * glm::vec4(point, 1.0f);
			if (clip.w <= 1e-6f)
				return false;

			glm::vec3 ndc = glm::vec3(clip) / clip.w;
			outScreen = ImVec2(viewportPos.x + (ndc.x * 0.5f + 0.5f) * viewportSize.x,
							   viewportPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewportSize.y);
			return true;
		}

		void DrawTextWithShadow(ImDrawList* drawList, const ImVec2& pos, ImU32 color, const char* text) {
			drawList->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), IM_COL32(0, 0, 0, 200), text);
			drawList->AddText(pos, color, text);
		}
	}

	Application::Application() : m_Window(nullptr), m_Height(900), m_Width(1600), m_IsRunning(true),
								 m_Camera(45.0f, 0.1f, 100.0f) {
		s_Instance = this;

		m_World.LoadScene();


	}

	Application::~Application() {
		//Shutdown();
		m_World.DestroyScene();
		s_Instance = nullptr;
	}

	Application &Application::Get() {
		return *s_Instance;
	}

	bool Application::Initialize(int width, int height) {
		m_Width = width;
		m_Height = height;

		if (!glfwInit())
			return false;

		// Decide GL+GLSL versions
	#if defined(IMGUI_IMPL_OPENGL_ES2)
		// GL ES 2.0 + GLSL 100
		const char* glsl_version = "#version 100";
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
		glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
	#elif defined(__APPLE__)
		// GL 3.2 + GLSL 150
		const char* glsl_version = "#version 150";
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
	#else
		// GL 3.0 + GLSL 130
		const char* glsl_version = "#version 430";
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
	#endif

		// Create window with graphics context
		m_Window = glfwCreateWindow(width, height, "PathTracer", nullptr, nullptr);

		if (m_Window == nullptr)
			return false;

		int bufferW, bufferH;
		glfwGetFramebufferSize(m_Window, &bufferW, &bufferH);

		glfwMakeContextCurrent(m_Window);
		glfwSwapInterval(1); // Enable vsync

		glewExperimental = true;

		if (glewInit() != GLEW_OK) {
			Shutdown();
			return false;
		}

		glViewport(0, 0, bufferW, bufferH);

		//m_Framebuffer = new FrameBuffer(bufferW, bufferH);

		std::cout << "" << std::endl;
		std::cout << "OpenGL Info" << std::endl;
		std::cout << "" << "OpenGL Vendor: " << glGetString(GL_VENDOR) << std::endl;
		std::cout << "" << "OpenGL Renderer: " << glGetString(GL_RENDERER) << std::endl;
		std::cout << "" << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
		std::cout << "" << "OpenGL Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
		std::cout << "" << std::endl;

		int max_compute_work_group_count[3];
		int max_compute_work_group_size[3];
		int max_compute_work_group_invocations;

		for (int idx = 0; idx < 3; idx++) {
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, idx, &max_compute_work_group_count[idx]);
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, idx, &max_compute_work_group_size[idx]);
		}

		glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &max_compute_work_group_invocations);

		std::cout << "" << std::endl;
		std::cout << "Compute Shader Info" << std::endl;
		std::cout << "Max global (total) work group counts X: " << max_compute_work_group_count[0] << ", Y: " << max_compute_work_group_count[1] << ", Z: " << max_compute_work_group_count[2] << std::endl;
		std::cout << "Max local (in one shader) work group sizes X: " << max_compute_work_group_size[0] << ", Y: " << max_compute_work_group_size[1] << ", Z: " << max_compute_work_group_size[2] << std::endl;
		std::cout << "Max local work group invocations: " << max_compute_work_group_invocations << std::endl;
		std::cout << "" << std::endl;

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
		ImGui_ImplOpenGL3_Init(glsl_version);

		return true;
	}

	void Application::RunLoop() {
		ImGuiIO& io = ImGui::GetIO();

		m_Camera.OnResize(m_Width, m_Height);
		m_Renderer.OnResize(m_Width, m_Height);
		m_Renderer.Initialize(m_Camera, m_World);

		while (!glfwWindowShouldClose(m_Window) && m_IsRunning) {
			glfwPollEvents();

			if (m_Camera.OnUpdate(m_DeltaTime))
				m_Renderer.ResetPathTracingCounter();

			ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

			// Start the Dear ImGui frame
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();

			glClearColor(0.0, 0.0, 0.0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT);

			ImGui::NewFrame();
			ImGuizmo::BeginFrame();

			{
				static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

				ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

				const ImGuiViewport* viewport = ImGui::GetMainViewport();
				ImGui::SetNextWindowPos(viewport->WorkPos);
				ImGui::SetNextWindowSize(viewport->WorkSize);
				ImGui::SetNextWindowViewport(viewport->ID);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
				window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
				window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

				if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
					window_flags |= ImGuiWindowFlags_NoBackground;

				ImGui::Begin("DockSpace Demo", nullptr, window_flags);

				ImGui::PopStyleVar(2);

				// Submit the DockSpace
				ImGuiIO& io = ImGui::GetIO();
				if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
					ImGuiID dockspace_id = ImGui::GetID("OpenGL Dockspace");
					ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
				}

				ImGui::End();
			}

			// Rendering
			// RenderUI disegna la finestra "Viewport", e con essa overlay e gizmo
			RenderUI(m_DeltaTime);
			UpdateSelection();
			// dopo UpdateSelection: e' li' che Canc chiama OpenPopup, cosi' la modale
			// compare nello stesso frame in cui viene richiesta
			DrawDeletePopup();
			Render(m_DeltaTime);
			ImGui::Render();

			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
				GLFWwindow* backup_current_context = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backup_current_context);
			}

			CalculateTime();

			glfwSwapBuffers(m_Window);
		}
	}

	void Application::CalculateTime() {
		float time = glfwGetTime();
		m_Timer = time - m_ResetTimer;
		m_DeltaTime = time - m_LastFrameTime;
		m_NFrames++;

		if (m_Timer >= 1.0) {
			m_FPS = (double)m_NFrames / m_Timer;
			m_NFrames = 0;
			m_ResetTimer = time;
		}
		
		m_LastFrameTime = time;
	}

	void Application::Shutdown() {
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		glfwDestroyWindow(m_Window);
		glfwTerminate();

		m_IsRunning = false;
	}

	void Application::RenderUI(float deltaTime) {
		ValidateSelection();

		ImGui::Begin("Settings");

		ImGui::Text("FPS: %.1f", m_FPS);
		ImGui::SameLine();
		ImGui::TextDisabled("| %.2f ms/frame", deltaTime * 1000.0f);
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80.0f);
		if (ImGui::Button("Screenshot"))
			m_Renderer.GetFrameBuffer()->SavePPMTexture();

		ImGui::Spacing();

		if (ImGui::BeginTabBar("MainTabs")) {
			if (ImGui::BeginTabItem("Scene")) {
				DrawSceneTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Materials")) {
				DrawMaterialsTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Render")) {
				DrawRenderTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Debug")) {
				DrawDebugTab();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();

		// finestra a se' stante: va aperta fuori dal Begin/End di "Settings"
		DrawMaterialEditorWindow();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("Viewport");
		m_ViewportWidth = (uint32_t)ImGui::GetContentRegionAvail().x;
		m_ViewportHeight = (uint32_t)ImGui::GetContentRegionAvail().y;

		ImVec2 pos = ImGui::GetCursorScreenPos();
		auto image = m_Renderer.GetFrameBuffer()->GetTexture();
		if(image != 0)
			ImGui::GetWindowDrawList()->AddImage(
				image,
				ImVec2(pos.x, pos.y),
				ImVec2(pos.x + m_ViewportWidth, pos.y + m_ViewportHeight),
				ImVec2(0, 1),
				ImVec2(1, 0)
			);

		// rettangolo dell'immagine: a questo si ancorano picking, gizmo e overlay
		m_ViewportPos = pos;
		m_ViewportSize = ImVec2((float)m_ViewportWidth, (float)m_ViewportHeight);
		m_ViewportHovered = ImGui::IsWindowHovered();

		// disegnati QUI dentro, sulla draw list della finestra "Viewport": la background
		// draw list starebbe sotto l'immagine e non si vedrebbe nulla
		DrawSelectionOverlay();
		DrawGizmo();

		ImGui::End();
		ImGui::PopStyleVar();
	}

	// ---------------------------------------------------------------- UI: tab Scene

	void Application::DrawSceneTab() {
		ImGui::Spacing();

		if (ImGui::Combo("Scene", &m_World.CurrentScene,
			"Two Spheres\0Random Spheres\0Cornell Box\0Random Boxes\0Cornell Box (meshes)\0Setup 1\0Setup 2\0Setup 3\0\0")) {
			// gli indici della vecchia scena non significano piu' niente: la finestra
			// "Viewport" viene disegnata piu' avanti in QUESTO stesso frame e userebbe
			// la selezione stale per indicizzare collezioni gia' svuotate
			m_Selection = {};

			m_World.DestroyScene();
			m_Renderer.ResetPathTracingCounter(true);
			m_World.LoadScene();
		}

		if (ImGui::Checkbox("Environment Mapping", &m_Renderer.EnvironmentMapping))
			m_Renderer.ResetPathTracingCounter();

		if (!m_Renderer.EnvironmentMapping)
			if (ImGui::ColorEdit3("Background", glm::value_ptr(m_World.BackgroundColor)))
				m_Renderer.ResetPathTracingCounter();

		ImGui::SeparatorText("Add");
		DrawAddObjectMenu();

		ImGui::SeparatorText("Outliner");
		DrawOutliner();

		ImGui::SeparatorText("Properties");
		DrawSelectionProperties();
	}

	void Application::DrawOutliner() {
		// I 6 quad che compongono un box vivono ANCHE in Quads: elencarli entrambi
		// riempirebbe l'outliner di duplicati (Random Boxes ha 121 box = 726 quad).
		// Le facce si selezionano tramite il box che le possiede.
		std::vector<bool> quadOwnedByBox(m_World.Quads.size(), false);
		for (const Box& box : m_World.Boxes) {
			int first = static_cast<int>(box.index);
			for (int q = first; q < first + 6 && q < (int)m_World.Quads.size(); q++)
				quadOwnedByBox[q] = true;
		}

		const size_t standaloneQuads = std::count(quadOwnedByBox.begin(), quadOwnedByBox.end(), false);
		const bool empty = m_World.Spheres.empty() && m_World.Boxes.empty()
						&& standaloneQuads == 0 && m_World.Meshes.empty();

		ImGui::BeginChild("OutlinerList", ImVec2(0.0f, 190.0f), true);

		if (empty)
			ImGui::TextDisabled("scena vuota");

		if (!m_World.Spheres.empty() && ImGui::TreeNodeEx("spheres", ImGuiTreeNodeFlags_DefaultOpen, "Spheres (%zu)", m_World.Spheres.size())) {
			for (size_t i = 0; i < m_World.Spheres.size(); i++)
				SelectableObjectRow("Sphere", SelectionType::Sphere, (int)i, m_World.Spheres[i].MaterialIndex);
			ImGui::TreePop();
		}

		if (!m_World.Boxes.empty() && ImGui::TreeNodeEx("boxes", ImGuiTreeNodeFlags_DefaultOpen, "Boxes (%zu)", m_World.Boxes.size())) {
			for (size_t i = 0; i < m_World.Boxes.size(); i++)
				SelectableObjectRow("Box", SelectionType::Box, (int)i, m_World.Boxes[i].MaterialIndex);
			ImGui::TreePop();
		}

		if (standaloneQuads > 0 && ImGui::TreeNodeEx("quads", ImGuiTreeNodeFlags_DefaultOpen, "Quads (%zu)", standaloneQuads)) {
			for (size_t i = 0; i < m_World.Quads.size(); i++) {
				if (quadOwnedByBox[i])
					continue; // faccia di un box

				SelectableObjectRow("Quad", SelectionType::Quad, (int)i, m_World.Quads[i].MaterialIndex);
			}
			ImGui::TreePop();
		}

		if (!m_World.Meshes.empty() && ImGui::TreeNodeEx("meshes", ImGuiTreeNodeFlags_DefaultOpen, "Meshes (%zu)", m_World.Meshes.size())) {
			for (size_t i = 0; i < m_World.Meshes.size(); i++) {
				const char* name = i < m_World.MeshNames.size() ? m_World.MeshNames[i].c_str() : "Mesh";
				SelectableObjectRow(name, SelectionType::Mesh, (int)i, m_World.Meshes[i].MaterialIndex);
			}
			ImGui::TreePop();
		}

		ImGui::EndChild();
	}

	// Una riga dell'outliner: cliccarla equivale a cliccare l'oggetto nella viewport,
	// cosi' selezione, gizmo e contorno restano un'unica nozione.
	bool Application::SelectableObjectRow(const char* label, SelectionType type, int index, float materialIndex) {
		ImGui::PushID(static_cast<int>(type) * 100000 + index);

		const bool selected = (m_Selection.Type == type && m_Selection.Index == index);

		char row[160];
		snprintf(row, sizeof(row), "%s #%d", label, index);

		const bool clicked = ImGui::Selectable(row, selected);

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30.0f);
		ImGui::TextDisabled("m%d", static_cast<int>(materialIndex));

		if (clicked) {
			m_Selection = { type, index };
			m_GizmoOperation = ImGuizmo::TRANSLATE;
		}

		ImGui::PopID();
		return clicked;
	}

	void Application::DrawGizmoToolbar() {
		const ImGuizmo::OPERATION effective = EffectiveGizmoOperation();

		auto operationButton = [&](const char* label, ImGuizmo::OPERATION operation, bool enabled) {
			const bool active = (effective == operation);
			if (active)
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.55f, 0.10f, 1.0f));

			ImGui::BeginDisabled(!enabled);
			if (ImGui::Button(label))
				m_GizmoOperation = operation;
			ImGui::EndDisabled();

			if (active)
				ImGui::PopStyleColor();
		};

		operationButton("Move (G)", ImGuizmo::TRANSLATE, true);
		ImGui::SameLine();
		operationButton("Rotate (R)", ImGuizmo::ROTATE, m_Selection.SupportsRotation());
		ImGui::SameLine();
		operationButton("Scale (S)", ImGuizmo::SCALE, true);
	}

	void Application::DrawSelectionProperties() {
		if (!m_Selection.IsValid()) {
			ImGui::TextDisabled("Nessun oggetto selezionato.");
			ImGui::TextDisabled("Click nella viewport, o su una riga dell'outliner.");
			return;
		}

		ImGui::TextColored(ImVec4(1.0f, 0.67f, 0.16f, 1.0f), "%s", GetSelectionLabel().c_str());
		DrawGizmoToolbar();
		ImGui::Spacing();

		ImGui::PushItemWidth(200.0f);

		switch (m_Selection.Type) {
		case SelectionType::Sphere: {
			Sphere& sphere = m_World.Spheres.at(m_Selection.Index);
			if (ImGui::DragFloat3("Center", glm::value_ptr(sphere.Position), 0.1f)) m_Renderer.ResetPathTracingCounter();
			if (ImGui::DragFloat("Radius", &sphere.Position.w, 0.05f, 0.001f, 1000.0f)) m_Renderer.ResetPathTracingCounter();
			if (MaterialCombo("Material", sphere.MaterialIndex)) m_Renderer.ResetPathTracingCounter();
			break;
		}
		case SelectionType::Quad: {
			Quad& quad = m_World.Quads.at(m_Selection.Index);
			if (ImGui::DragFloat3("Corner", glm::value_ptr(quad.PositionLLC), 0.1f)) m_Renderer.ResetPathTracingCounter();
			if (ImGui::DragFloat3("U axis", glm::value_ptr(quad.U), 0.05f)) m_Renderer.ResetPathTracingCounter();
			if (ImGui::DragFloat3("V axis", glm::value_ptr(quad.V), 0.05f)) m_Renderer.ResetPathTracingCounter();
			if (ImGui::DragFloat("Width", &quad.Width, 0.1f, 0.001f, 1000.0f)) m_Renderer.ResetPathTracingCounter();
			if (ImGui::DragFloat("Height", &quad.Height, 0.1f, 0.001f, 1000.0f)) m_Renderer.ResetPathTracingCounter();
			if (MaterialCombo("Material", quad.MaterialIndex)) m_Renderer.ResetPathTracingCounter();
			ImGui::TextDisabled("pivot = angolo inferiore sinistro");
			break;
		}
		case SelectionType::Box: {
			Box& box = m_World.Boxes.at(m_Selection.Index);

			bool changed = false;
			changed |= ImGui::DragFloat3("Min", glm::value_ptr(box.Min), 0.1f);
			changed |= ImGui::DragFloat3("Max", glm::value_ptr(box.Max), 0.1f);
			changed |= MaterialCombo("Material", box.MaterialIndex);

			if (changed) {
				box.UpdateBox(m_World.Quads); // un box e' 6 quad: vanno riscritti
				m_Renderer.ResetPathTracingCounter();
			}
			ImGui::TextDisabled("axis-aligned: non ruotabile");
			break;
		}
		case SelectionType::Mesh: {
			MeshInfo& mesh = m_World.Meshes.at(m_Selection.Index);
			ImGui::Text("Triangoli: %d", static_cast<int>(mesh.NumTriangles));

			glm::vec3 position = glm::vec3(mesh.Transform[3]); // la traslazione e' l'ultima colonna
			if (ImGui::DragFloat3("Position", glm::value_ptr(position), 0.1f)) {
				glm::mat4 transform = mesh.Transform;
				transform[3] = glm::vec4(position, 1.0f);
				m_World.SetMeshTransform(m_Selection.Index, transform);
				m_Renderer.ResetPathTracingCounter();
			}

			if (MaterialCombo("Material", mesh.MaterialIndex)) m_Renderer.ResetPathTracingCounter();

			if (ImGui::Button("Reset transform")) {
				m_World.SetMeshTransform(m_Selection.Index, glm::mat4(1.0f));
				m_Renderer.ResetPathTracingCounter();
			}
			break;
		}
		default:
			break;
		}

		ImGui::PopItemWidth();

		ImGui::Spacing();
		if (ImGui::Button("Deseleziona (ESC)"))
			m_Selection = {};

		ImGui::SameLine();
		if (ImGui::Button("Elimina (Canc)"))
			m_DeleteRequested = true;
	}

	// Un DragFloat sull'indice del materiale non dice niente su cosa si sta scegliendo:
	// qui ogni voce mostra la pastiglia col colore del materiale.
	bool Application::MaterialCombo(const char* label, float& materialIndex) {
		if (m_World.Materials.empty())
			return false;

		const int count = static_cast<int>(m_World.Materials.size());
		const int current = std::clamp(static_cast<int>(materialIndex), 0, count - 1);

		const std::string preview = "Material " + std::to_string(current);
		bool changed = false;

		if (ImGui::BeginCombo(label, preview.c_str())) {
			for (int i = 0; i < count; i++) {
				ImGui::PushID(i);

				const glm::vec4& color = m_World.Materials[i].Color;
				ImGui::ColorButton("##swatch", ImVec4(color.r, color.g, color.b, 1.0f),
					ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(14.0f, 14.0f));
				ImGui::SameLine();

				const std::string item = "Material " + std::to_string(i);
				if (ImGui::Selectable(item.c_str(), i == current)) {
					materialIndex = static_cast<float>(i);
					changed = true;
				}

				ImGui::PopID();
			}
			ImGui::EndCombo();
		}

		// Scorciatoia verso l'editor di QUESTO materiale: evita di cambiare tab,
		// ritrovare l'indice giusto e riaprire l'albero.
		ImGui::SameLine();
		ImGui::PushID(label);
		if (ImGui::SmallButton("Edit"))
			OpenMaterialEditor(std::clamp(static_cast<int>(materialIndex), 0, count - 1));
		ImGui::PopID();

		return changed;
	}

	// Nuovi oggetti nascono davanti alla camera, non nell'origine: altrimenti finirebbero
	// fuori vista e sembrerebbe che il pulsante non abbia fatto niente.
	glm::vec3 Application::SpawnPosition() const {
		return m_Camera.GetPosition() + m_Camera.GetDirection() * 4.0f;
	}

	// Un oggetto con un MaterialIndex senza corrispondenza indicizzerebbe fuori dal
	// buffer dei materiali nello shader.
	int Application::EnsureMaterialExists() {
		if (m_World.Materials.empty())
			m_World.Materials.push_back(CreateDefaultPrincipled());

		return std::clamp(static_cast<int>(m_NewObjectMaterial), 0, (int)m_World.Materials.size() - 1);
	}

	void Application::DrawAddObjectMenu() {
		const glm::vec3 spawn = SpawnPosition();

		if (ImGui::Button("+ Sphere")) {
			const int material = EnsureMaterialExists();

			Sphere sphere;
			sphere.Position = glm::vec4(spawn, 0.5f); // la w e' il raggio
			sphere.MaterialIndex = static_cast<float>(material);
			m_World.Spheres.push_back(sphere);

			m_Selection = { SelectionType::Sphere, (int)m_World.Spheres.size() - 1 };
			m_GizmoOperation = ImGuizmo::TRANSLATE;
			m_Renderer.ResetPathTracingCounter();
		}

		ImGui::SameLine();
		if (ImGui::Button("+ Box")) {
			const int material = EnsureMaterialExists();

			// CreateBox appende i suoi 6 quad in coda, quindi i Box::index gia' esistenti
			// restano validi e in ordine crescente, come si aspetta il loop dei Cubes
			// nello shader (che li scorre in parallelo ai quad).
			m_World.CreateBox(spawn - glm::vec3(0.5f), spawn + glm::vec3(0.5f), (float)material);

			m_Selection = { SelectionType::Box, (int)m_World.Boxes.size() - 1 };
			m_GizmoOperation = ImGuizmo::TRANSLATE;
			m_Renderer.ResetPathTracingCounter();
		}

		ImGui::SameLine();
		if (ImGui::Button("+ Quad")) {
			const int material = EnsureMaterialExists();

			Quad quad;
			quad.PositionLLC = glm::vec4(spawn - glm::vec3(0.5f, 0.5f, 0.0f), 0.0f);
			quad.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			quad.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			quad.Width = 1.0f;
			quad.Height = 1.0f;
			quad.MaterialIndex = static_cast<float>(material);
			m_World.Quads.push_back(quad);

			m_Selection = { SelectionType::Quad, (int)m_World.Quads.size() - 1 };
			m_GizmoOperation = ImGuizmo::TRANSLATE;
			m_Renderer.ResetPathTracingCounter();
		}

		// elenco dei .obj disponibili, letto una volta sola
		if (m_ModelFiles.empty() && std::filesystem::exists("models")) {
			for (const auto& entry : std::filesystem::directory_iterator("models"))
				if (entry.path().extension() == ".obj")
					m_ModelFiles.push_back(entry.path().filename().string());

			std::sort(m_ModelFiles.begin(), m_ModelFiles.end());
		}

		if (!m_ModelFiles.empty()) {
			ImGui::PushItemWidth(160.0f);

			if (ImGui::BeginCombo("##model", m_ModelFiles[m_SelectedModelFile].c_str())) {
				for (int i = 0; i < (int)m_ModelFiles.size(); i++)
					if (ImGui::Selectable(m_ModelFiles[i].c_str(), i == m_SelectedModelFile))
						m_SelectedModelFile = i;

				ImGui::EndCombo();
			}

			ImGui::PopItemWidth();
			ImGui::SameLine();

			if (ImGui::Button("+ Mesh")) {
				const int material = EnsureMaterialExists();
				const std::string path = "models/" + m_ModelFiles[m_SelectedModelFile];

				// ricostruisce l'INTERO BVH: sui modelli grossi puo' bloccare per qualche secondo
				const int mesh = m_World.AddMesh(path, spawn, material);
				if (mesh >= 0) {
					m_Selection = { SelectionType::Mesh, mesh };
					m_GizmoOperation = ImGuizmo::TRANSLATE;
				}

				m_Renderer.ResetPathTracingCounter();
			}
		}

		ImGui::PushItemWidth(160.0f);
		MaterialCombo("Material for new objects", m_NewObjectMaterial);
		ImGui::PopItemWidth();
	}

	// ------------------------------------------------------------ UI: tab Materials

	// Quanti elementi ha la collezione a cui punta una selezione di questo tipo.
	int Application::SelectionCollectionSize(SelectionType type) const {
		switch (type) {
		case SelectionType::Sphere: return static_cast<int>(m_World.Spheres.size());
		case SelectionType::Quad:   return static_cast<int>(m_World.Quads.size());
		case SelectionType::Box:    return static_cast<int>(m_World.Boxes.size());
		case SelectionType::Mesh:   return static_cast<int>(m_World.Meshes.size());
		default:                    return 0;
		}
	}

	bool Application::CanDeleteSelection() const {
		return m_Selection.IsValid() && m_Selection.Index < SelectionCollectionSize(m_Selection.Type);
	}

	// Ogni rimozione fa scalare gli indici successivi della sua collezione: la selezione
	// corrente diventa insensata comunque vada, quindi si azzera PRIMA di toccare il World
	// (la finestra "Viewport" la userebbe piu' avanti in questo stesso frame).
	void Application::DeleteSelection() {
		if (!CanDeleteSelection()) return;

		const Selection sel = m_Selection;
		m_Selection = {};

		bool removed = false;
		switch (sel.Type) {
		case SelectionType::Sphere: removed = m_World.RemoveSphere(sel.Index); break;
		case SelectionType::Quad:   removed = m_World.RemoveQuad(sel.Index);   break;
		case SelectionType::Box:    removed = m_World.RemoveBox(sel.Index);    break;
		case SelectionType::Mesh:   removed = m_World.RemoveMesh(sel.Index);   break;
		default: break;
		}

		if (removed)
			m_Renderer.ResetPathTracingCounter();
	}

	// Modale: va aperta fuori dal Begin/End di qualsiasi finestra.
	// m_ShowDeletePopup e' solo il nostro specchio dello stato di ImGui: la OpenPopup
	// la fa chi preme Canc, e se ImGui chiude da solo (Escape, click fuori) BeginPopupModal
	// torna false e qui si risincronizza. Riaprirla in base al flag sarebbe un loop.
	void Application::DrawDeletePopup() {
		if (m_DeleteRequested) {
			m_DeleteRequested = false;
			m_ShowDeletePopup = true;
			ImGui::OpenPopup(kDeletePopupId);
		}

		if (!m_ShowDeletePopup)
			return;

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(kDeletePopupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			// La selezione puo' essere svanita mentre la modale era aperta (cambio scena):
			// stessa classe di bug che faceva crashare la selezione, stesso trattamento.
			if (!CanDeleteSelection()) {
				m_ShowDeletePopup = false;
				ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
				return;
			}

			ImGui::Text("Eliminare %s?", GetSelectionName().c_str());

			switch (m_Selection.Type) {
			case SelectionType::Mesh:
				ImGui::TextDisabled("%d triangoli - il BVH verra' ricostruito",
					static_cast<int>(m_World.Meshes[m_Selection.Index].NumTriangles));
				break;
			case SelectionType::Box:
				ImGui::TextDisabled("porta via anche le sue 6 facce");
				break;
			default:
				break;
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Elimina", ImVec2(120, 0))) {
				DeleteSelection();
				m_ShowDeletePopup = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button("Annulla", ImVec2(120, 0))) {
				m_ShowDeletePopup = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
		else {
			// ImGui l'ha chiusa per conto suo: Escape o click all'esterno
			m_ShowDeletePopup = false;
		}
	}

	// Corpo dell'editor, condiviso fra il tab Materials e la finestra flottante:
	// due copie divergerebbero al primo campo aggiunto.
	void Application::DrawMaterialEditor(Material& material, int index) {
		// Editor in stile Principled (Blender): mostra concetti da artista e scrive nei campi
		// raw dello struct, che restano il contratto con lo shader. Le "probabilita'" di
		// importance sampling non compaiono: le derivo qui. Cosi' non possono piu' sommare
		// a un valore che sopprime il diffuso, che era il footgun della UI precedente.
		bool changed = false;
		const bool isMetal = material.Metalness > 0.001f;

		// Base color. La tinta del vetro segue la base color (assorbimento Beer-Lambert).
		glm::vec3 baseColor = glm::vec3(material.Color);
		if (ImGui::ColorEdit3("Base Color", glm::value_ptr(baseColor))) {
			material.Color = glm::vec4(baseColor, 1.0f);
			changed = true;
		}

		ImGui::SeparatorText("Surface");

		if (ImGui::SliderFloat("Metallic", &material.Metalness, 0.0f, 1.0f)) changed = true;

		// Un'unica roughness per riflesso e trasmissione, come il Principled.
		if (ImGui::SliderFloat("Roughness", &material.Roughness, 0.0f, 1.0f)) {
			material.RefractionRoughness = material.Roughness;
			changed = true;
		}

		// Specular tint: colora il riflesso di un dielettrico verso la base color. Un metallo
		// lo ignora (il riflesso e' gia' tinto dall'albedo), quindi si grigia a metallo pieno.
		ImGui::BeginDisabled(material.Metalness >= 0.999f);
		if (ImGui::SliderFloat("Specular tint", &material.SpecularTint, 0.0f, 1.0f)) changed = true;
		ImGui::EndDisabled();

		// IOR: il Fresnel lo usa anche per il riflesso dielettrico, non solo per il vetro.
		// Su un metallo non ha effetto (rifrazione spenta, Fresnel saturo).
		ImGui::BeginDisabled(isMetal);
		if (ImGui::SliderFloat("IOR", &material.RefractionRatio, 1.0f, 2.5f)) changed = true;

		// Transmission = quanto e' vetroso. Un metallo non trasmette.
		float transmission = material.RefractionProbability;
		if (ImGui::SliderFloat("Transmission", &transmission, 0.0f, 1.0f)) {
			material.RefractionProbability = transmission;
			changed = true;
		}
		ImGui::EndDisabled();
		if (isMetal)
			ImGui::TextDisabled("metallo: niente diffuso ne' trasmissione, riflesso tinto dall'albedo");

		// Specular level (F0 dielettrico) sepolto in Advanced, come in Blender: quasi
		// nessuno lo tocca. F0 = 0.08 * livello, quindi 0.5 -> 0.04 (dielettrico standard).
		if (ImGui::TreeNode("Advanced")) {
			ImGui::BeginDisabled(isMetal);
			float specLevel = std::clamp(material.SpecularProbability / 0.08f, 0.0f, 1.0f);
			if (ImGui::SliderFloat("Specular", &specLevel, 0.0f, 1.0f)) {
				material.SpecularProbability = 0.08f * specLevel;
				changed = true;
			}
			ImGui::EndDisabled();
			ImGui::TextDisabled("F0 dielettrico. 0.5 = 0.04, il valore fisico standard");
			ImGui::TreePop();
		}

		// I campi derivati si "curano" ad ogni modifica: il riflesso dielettrico e' bianco
		// (non tinto) e la tinta del vetro segue la base color. Legacy con SpecularColor
		// colorato vengono normalizzati alla prima modifica.
		if (changed) {
			material.SpecularColor = glm::vec4(1.0f);
			material.RefractionColor = material.Color;
			m_Renderer.ResetPathTracingCounter();
		}

		ImGui::SeparatorText("Emission");
		if (ImGui::DragFloat("Emissive Strength", &material.EmissiveStrenght, 0.1f, 0.0f, FLT_MAX)) m_Renderer.ResetPathTracingCounter();
		if (ImGui::ColorEdit3("Emissive Color", glm::value_ptr(material.EmissiveColor))) m_Renderer.ResetPathTracingCounter();

		ImGui::SeparatorText("Base Color texture");
		int checkerMode = static_cast<int>(material.Checker + 0.5f);
		if (ImGui::Combo("Checker", &checkerMode, "Off\0UV (debug)\0World space\0\0")) {
			material.Checker = static_cast<float>(checkerMode);
			m_Renderer.ResetPathTracingCounter();
		}
		if (checkerMode != 0) {
			if (ImGui::DragFloat("Checker scale", &material.CheckerScale, 0.1f, 0.01f, 64.0f))
				m_Renderer.ResetPathTracingCounter();
		}

		// Nello shader checker e texture sono in catena else-if: il checker vince e la texture
		// non viene mai letta. Invece di mostrarli come se si combinassero, la disabilito e lo
		// dico, cosi' l'esclusione non e' piu' silenziosa.
		const bool checkerWins = checkerMode != 0;
		const int layerCount = static_cast<int>(m_World.TexturePaths.size());
		ImGui::BeginDisabled(checkerWins);
		if (layerCount > 0) {
			// -1 = nessuna texture; gli altri valori indicizzano i layer di TexturePaths.
			// Con min == max ImGui disattiva il clamping, quindi senza texture
			// il drag va nascosto del tutto invece che lasciato libero di sforare.
			if (ImGui::DragFloat("Albedo texture layer", &material.AlbedoTexture, 1.0f, -1.0f, (float)(layerCount - 1)))
				m_Renderer.ResetPathTracingCounter();
		}
		else {
			material.AlbedoTexture = -1.0f;
			ImGui::TextDisabled("(nessuna texture caricata in questa scena)");
		}
		ImGui::EndDisabled();
		if (checkerWins)
			ImGui::TextDisabled("checker attivo: ha la precedenza, la texture e' ignorata");

		ImGui::SeparatorText("Procedural noise");
		int noiseMode = static_cast<int>(material.NoiseType + 0.5f);
		if (ImGui::Combo("Noise", &noiseMode, "Off\0Perlin\0Turbulence\0Marble\0\0")) {
			material.NoiseType = static_cast<float>(noiseMode);
			m_Renderer.ResetPathTracingCounter();
		}
		if (noiseMode != 0) {
			if (ImGui::DragFloat("Noise scale", &material.NoiseScale, 0.05f, 0.01f, 64.0f)) m_Renderer.ResetPathTracingCounter();
			if (ImGui::SliderFloat("Noise blend", &material.NoiseBlend, 0.0f, 1.0f)) m_Renderer.ResetPathTracingCounter();

			// Turbulence e Marble sommano ottave: il costo per raggio e' lineare in questo
			// valore, e viene pagato a ogni rimbalzo su questo materiale.
			if (noiseMode >= 2) {
				int octaves = static_cast<int>(material.NoiseOctaves + 0.5f);
				if (ImGui::SliderInt("Octaves", &octaves, 1, 8)) {
					material.NoiseOctaves = static_cast<float>(octaves);
					m_Renderer.ResetPathTracingCounter();
				}
			}
			// La fase e' quello che trasforma il rumore in venature: senza, il marmo e' nebbia.
			if (noiseMode == 3) {
				if (ImGui::DragFloat("Turbulence phase", &material.NoiseTurbulence, 0.1f, 0.0f, 40.0f))
					m_Renderer.ResetPathTracingCounter();
			}

			if (ImGui::ColorEdit3("Noise color A", glm::value_ptr(material.NoiseColorA))) m_Renderer.ResetPathTracingCounter();
			if (ImGui::ColorEdit3("Noise color B", glm::value_ptr(material.NoiseColorB))) m_Renderer.ResetPathTracingCounter();

			ImGui::TextDisabled("Valutato in world space: l'oggetto \"scorre\" nel rumore se lo sposti");
		}

		ImGui::Spacing();
		ImGui::TextDisabled("Material %d", index);
	}

	void Application::OpenMaterialEditor(int index) {
		m_MaterialEditorIndex = index;
		m_ShowMaterialEditor = true;
	}

	// Finestra flottante sul materiale dell'oggetto selezionato.
	void Application::DrawMaterialEditorWindow() {
		if (!m_ShowMaterialEditor)
			return;

		// L'indice puo' essere diventato invalido (cambio scena): e' esattamente la classe
		// di bug che ha fatto crashare la selezione, quindi stesso trattamento.
		if (m_MaterialEditorIndex < 0 || m_MaterialEditorIndex >= (int)m_World.Materials.size()) {
			m_ShowMaterialEditor = false;
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_FirstUseEver);

		// il "###" fissa l'ID della finestra mentre il titolo cambia con l'indice
		const std::string title = "Material " + std::to_string(m_MaterialEditorIndex) + "###MaterialEditor";

		if (ImGui::Begin(title.c_str(), &m_ShowMaterialEditor)) {
			ImGui::PushItemWidth(170.0f);
			DrawMaterialEditor(m_World.Materials[m_MaterialEditorIndex], m_MaterialEditorIndex);
			ImGui::PopItemWidth();
		}

		ImGui::End();
	}

	// I materiali indicizzano le texture per LAYER (Material::AlbedoTexture). Rimuoverne una
	// farebbe scalare i layer successivi e ogni materiale punterebbe all'immagine sbagliata,
	// in silenzio. Quindi qui si puo' solo aggiungere in coda, come per gli oggetti.
	void Application::DrawTextureManager() {
		ImGui::SeparatorText("Textures");

		if (m_World.TexturePaths.empty())
			ImGui::TextDisabled("nessuna texture caricata");

		for (size_t i = 0; i < m_World.TexturePaths.size(); i++)
			ImGui::BulletText("layer %d  -  %s", (int)i, m_World.TexturePaths[i].c_str());

		// elenco dei file disponibili, letto una volta sola. EnvironmentMap e' un'altra cosa:
		// finisce nella cubemap, non nel sampler2DArray degli albedo.
		if (m_TextureFiles.empty() && std::filesystem::exists("textures")) {
			for (const auto& entry : std::filesystem::recursive_directory_iterator("textures")) {
				if (!entry.is_regular_file()) continue;

				const std::string path = entry.path().string();
				if (path.find("EnvironmentMap") != std::string::npos) continue;

				const std::string ext = entry.path().extension().string();
				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp")
					m_TextureFiles.push_back(path);
			}

			std::sort(m_TextureFiles.begin(), m_TextureFiles.end());
		}

		if (m_TextureFiles.empty()) {
			ImGui::TextDisabled("(nessun file in res/textures)");
			return;
		}

		m_SelectedTextureFile = std::clamp(m_SelectedTextureFile, 0, (int)m_TextureFiles.size() - 1);

		ImGui::PushItemWidth(240.0f);
		if (ImGui::BeginCombo("##texture", m_TextureFiles[m_SelectedTextureFile].c_str())) {
			for (int i = 0; i < (int)m_TextureFiles.size(); i++)
				if (ImGui::Selectable(m_TextureFiles[i].c_str(), i == m_SelectedTextureFile))
					m_SelectedTextureFile = i;

			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();

		const std::string& selected = m_TextureFiles[m_SelectedTextureFile];
		const bool alreadyLoaded = std::find(m_World.TexturePaths.begin(), m_World.TexturePaths.end(),
			selected) != m_World.TexturePaths.end();

		ImGui::BeginDisabled(alreadyLoaded);
		if (ImGui::Button("+ Texture")) {
			// PathTracer confronta TexturePaths con quelle caricate e ricarica da solo
			m_World.TexturePaths.push_back(selected);
			m_Renderer.ResetPathTracingCounter();
		}
		ImGui::EndDisabled();

		if (alreadyLoaded) {
			ImGui::SameLine();
			ImGui::TextDisabled("gia' caricata");
		}

		ImGui::TextDisabled("solo append: i layer sono indicizzati dai materiali");
	}

	void Application::DrawMaterialsTab() {
		ImGui::Spacing();

		DrawTextureManager();
		ImGui::Spacing();

		if (ImGui::Button("+ Add material")) {
			m_World.Materials.push_back(CreateDefaultPrincipled());
			OpenMaterialEditor((int)m_World.Materials.size() - 1);
			m_Renderer.ResetPathTracingCounter();
		}

		if (m_World.Materials.empty()) {
			ImGui::TextDisabled("nessun materiale in questa scena");
			return;
		}

		ImGui::Spacing();
		ImGui::PushItemWidth(200.0f);

		for (size_t i = 0; i < m_World.Materials.size(); i++) {
			Material& material = m_World.Materials.at(i);

			ImGui::PushID((int)i);

			// pastiglia del colore accanto al nome: si riconosce il materiale a colpo d'occhio
			ImGui::ColorButton("##swatch", ImVec4(material.Color.r, material.Color.g, material.Color.b, 1.0f),
				ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(14.0f, 14.0f));
			ImGui::SameLine();

			if (ImGui::TreeNode(("Material " + std::to_string(i)).c_str())) {
				DrawMaterialEditor(material, (int)i);
				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		ImGui::PopItemWidth();
	}

	// --------------------------------------------------------------- UI: tab Render

	void Application::DrawRenderTab() {
		ImGui::Spacing();
		ImGui::PushItemWidth(200.0f);

		ImGui::SeparatorText("Path tracing");
		ImGui::Checkbox("Enable Path Tracing", &m_Renderer.PathTracing);
		if (ImGui::SliderInt("Samples per pixel", &m_Renderer.m_SamplesPerPixel, 1, 16)) m_Renderer.ResetPathTracingCounter();
		if (ImGui::SliderInt("Ray depth", &m_Renderer.m_RayDepth, 1, 50)) m_Renderer.ResetPathTracingCounter();
		if (ImGui::Button("Reset Accumulation")) m_Renderer.ResetPathTracingCounter();

		ImGui::SeparatorText("Output");
		if (ImGui::Checkbox("Vsync", &m_Vsync))
			glfwSwapInterval(m_Vsync);

		// Esposizione e tonemap agiscono DOPO l'accumulo (fragment di post): cambiarli non
		// invalida i campioni gia' accumulati, quindi niente ResetPathTracingCounter.
		ImGui::Checkbox("ACES tonemap", &m_Renderer.PostProcessing);
		ImGui::DragFloat("Exposure", &m_Renderer.Exposure, 0.01f, 0.0f, 10.0f);

		// Il firefly clamp invece vive nel path tracer e cambia i campioni: va resettato.
		// Etichetta come luminanza massima; 0 = off. Sotto ~2 inizia a mangiare i riflessi.
		if (ImGui::DragFloat("Firefly clamp", &m_Renderer.FireflyClamp, 0.1f, 0.0f, 50.0f, "%.1f")) {
			if (m_Renderer.FireflyClamp < 0.0f) m_Renderer.FireflyClamp = 0.0f;
			m_Renderer.ResetPathTracingCounter();
		}
		if (m_Renderer.FireflyClamp <= 0.0f)
			ImGui::TextDisabled("firefly clamp off");

		ImGui::SeparatorText("Camera");
		if (ImGui::SliderFloat("Vertical FOV", &m_Camera.m_VerticalFOV, 30.0f, 140.0f)) {
			m_Camera.RecalculateProjection();
			m_Renderer.ResetPathTracingCounter();
		}
		if (ImGui::Button("Reset Camera Position")) {
			m_Camera.ResetPosition();
			m_Renderer.ResetPathTracingCounter();
		}
		ImGui::TextDisabled("tasto destro + WASD per navigare");

		ImGui::SeparatorText("Depth of field");
		// Apertura e fuoco cambiano i raggi PRIMARI: vanno resettati (a differenza
		// di esposizione/tonemap, che agiscono dopo l'accumulo).
		if (ImGui::SliderFloat("Aperture", &m_Renderer.Aperture, 0.0f, 0.5f, "%.3f")) {
			if (m_Renderer.Aperture < 0.0f) m_Renderer.Aperture = 0.0f;
			m_Renderer.ResetPathTracingCounter();
		}

		const bool dofOff = m_Renderer.Aperture <= 0.0f;
		ImGui::BeginDisabled(dofOff);
		if (ImGui::DragFloat("Focus distance", &m_Renderer.FocusDistance, 0.05f, 0.01f, 1000.0f, "%.2f")) {
			if (m_Renderer.FocusDistance < 0.01f) m_Renderer.FocusDistance = 0.01f;
			m_Renderer.ResetPathTracingCounter();
		}

		// pick del punto di fuoco: arma la modalita', il prossimo click nel viewport
		// mette a fuoco esattamente li'
		if (m_FocusPickArmed) {
			if (ImGui::Button("Annulla pick")) m_FocusPickArmed = false;
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "clicca un punto nella scena...");
		}
		else if (ImGui::Button("Pick focus (clicca un oggetto)")) {
			m_FocusPickArmed = true;
		}
		ImGui::EndDisabled();

		if (dofOff)
			ImGui::TextDisabled("apertura a 0: DOF disattivato (pinhole)");

		ImGui::PopItemWidth();

		ImGui::SeparatorText("OpenGL");
		ImGui::TextDisabled("Vendor:   %s", glGetString(GL_VENDOR));
		ImGui::TextDisabled("Renderer: %s", glGetString(GL_RENDERER));
		ImGui::TextDisabled("Version:  %s", glGetString(GL_VERSION));
	}

	// ---------------------------------------------------------------- UI: tab Debug

	void Application::DrawDebugTab() {
		ImGui::Spacing();

		ImGui::SeparatorText("BVH");
		if (ImGui::Checkbox("BVH Heatmap", &m_Renderer.BVHDebug))
			m_Renderer.ResetPathTracingCounter();

		if (m_Renderer.BVHDebug) {
			ImGui::TextDisabled("blu = poche ricerche, rosso = molte");
			ImGui::TextDisabled("disattiva Post Processing per i colori reali");
			ImGui::SliderFloat("Heat scale", &m_Renderer.BVHHeatScale, 1.0f, 256.0f);
		}

		ImGui::SeparatorText("Picking");
		ImGui::Text("hovered window : %s", HoveredWindowName());
		ImGui::Text("viewport hover : %s", m_ViewportHovered ? "SI" : "NO");
		ImGui::Text("viewport rect  : (%.0f,%.0f) %.0f x %.0f",
			m_ViewportPos.x, m_ViewportPos.y, m_ViewportSize.x, m_ViewportSize.y);
		ImGui::Text("richieste pick : %d", m_PickRequestCount);
		ImGui::Text("ultimo pixel   : (%d, %d)", m_LastPickPixel.x, m_LastPickPixel.y);
		ImGui::Text("ultimo esito   : type=%d index=%d", m_LastPickType, m_LastPickIndex);
		ImGui::TextDisabled("-2 = lo shader non ha scritto, -1 = raggio a vuoto");

		// Bypassa il gate del mouse: se questo seleziona ma il click no, il problema
		// e' nel gate; se non seleziona nemmeno questo, e' nella pipeline di picking.
		if (ImGui::Button("Pick al centro della viewport")) {
			m_LastPickPixel = { (int)m_ViewportSize.x / 2, (int)m_ViewportSize.y / 2 };
			m_PickRequestCount++;
			m_Renderer.RequestPick(m_LastPickPixel.x, m_LastPickPixel.y);
		}
	}

	// Dove ancorare il gizmo: il centro VISIVO dell'oggetto, non l'origine della sua
	// parametrizzazione (il pivot di un quad e' l'angolo, quello di una mesh e' l'origine
	// della sua Transform: entrambi possono cadere lontano dalla geometria).
	glm::vec3 Application::GetSelectionPivot() const {
		switch (m_Selection.Type) {
		case SelectionType::Sphere:
			return glm::vec3(m_World.Spheres.at(m_Selection.Index).Position);
		case SelectionType::Quad: {
			const Quad& q = m_World.Quads.at(m_Selection.Index);
			return glm::vec3(q.PositionLLC)
				 + glm::vec3(q.U) * (q.Width * 0.5f)
				 + glm::vec3(q.V) * (q.Height * 0.5f);
		}
		case SelectionType::Box: {
			const Box& b = m_World.Boxes.at(m_Selection.Index);
			return 0.5f * (glm::vec3(b.Min) + glm::vec3(b.Max));
		}
		case SelectionType::Mesh: {
			const MeshInfo& m = m_World.Meshes.at(m_Selection.Index);
			return 0.5f * (glm::vec3(m.BoundsMin) + glm::vec3(m.BoundsMax));
		}
		default:
			return glm::vec3(0.0f);
		}
	}

	// AABB world dell'oggetto selezionato, usata per disegnarne il contorno.
	void Application::GetSelectionBounds(glm::vec3& outMin, glm::vec3& outMax) const {
		outMin = glm::vec3(0.0f);
		outMax = glm::vec3(0.0f);

		switch (m_Selection.Type) {
		case SelectionType::Sphere: {
			const Sphere& s = m_World.Spheres.at(m_Selection.Index);
			glm::vec3 center(s.Position);
			outMin = center - glm::vec3(s.Position.w);
			outMax = center + glm::vec3(s.Position.w);
			break;
		}
		case SelectionType::Quad: {
			const Quad& q = m_World.Quads.at(m_Selection.Index);
			glm::vec3 llc(q.PositionLLC);
			glm::vec3 edgeU = glm::vec3(q.U) * q.Width;
			glm::vec3 edgeV = glm::vec3(q.V) * q.Height;

			// AABB dei 4 angoli del quad
			outMin = outMax = llc;
			for (const glm::vec3& corner : { llc + edgeU, llc + edgeV, llc + edgeU + edgeV }) {
				outMin = glm::min(outMin, corner);
				outMax = glm::max(outMax, corner);
			}
			break;
		}
		case SelectionType::Box: {
			const Box& b = m_World.Boxes.at(m_Selection.Index);
			outMin = glm::vec3(b.Min);
			outMax = glm::vec3(b.Max);
			break;
		}
		case SelectionType::Mesh: {
			const MeshInfo& m = m_World.Meshes.at(m_Selection.Index);
			outMin = glm::vec3(m.BoundsMin); // gia' in world space
			outMax = glm::vec3(m.BoundsMax);
			break;
		}
		default:
			break;
		}
	}

	// Sfere e box non sono ruotabili: se e' selezionata la rotazione si ripiega su
	// translate. Passa da qui sia il gizmo sia l'etichetta, cosi' non possono divergere.
	ImGuizmo::OPERATION Application::EffectiveGizmoOperation() const {
		if (m_GizmoOperation == ImGuizmo::ROTATE && !m_Selection.SupportsRotation())
			return ImGuizmo::TRANSLATE;

		return m_GizmoOperation;
	}

	// Solo il nome dell'oggetto, senza l'operazione del gizmo: serve anche alla modale
	// di cancellazione, dove "Eliminare Sphere #0 - Translate?" non avrebbe senso.
	std::string Application::GetSelectionName() const {
		std::string name;
		switch (m_Selection.Type) {
		case SelectionType::Sphere: name = "Sphere"; break;
		case SelectionType::Quad:   name = "Quad";   break;
		case SelectionType::Box:    name = "Box";    break;
		case SelectionType::Mesh:
			name = "Mesh";
			if (m_Selection.Index < (int)m_World.MeshNames.size())
				name += " \"" + m_World.MeshNames[m_Selection.Index] + "\"";
			break;
		default: return "?";
		}
		return name + " #" + std::to_string(m_Selection.Index);
	}

	std::string Application::GetSelectionLabel() const {
		if (!m_Selection.IsValid())
			return "Nessuna selezione  -  click su un oggetto per selezionarlo";

		const ImGuizmo::OPERATION effective = EffectiveGizmoOperation();
		const char* operation = effective == ImGuizmo::ROTATE ? "Rotate"
							  : effective == ImGuizmo::SCALE  ? "Scale" : "Translate";

		std::string label = GetSelectionName() + "  -  " + operation;
		if (!m_Selection.SupportsRotation())
			label += "  (no rotazione)";

		return label;
	}

	// Etichetta a schermo + contorno dell'AABB: feedback immediato su cosa e' selezionato,
	// utile perche' il gizmo da solo puo' finire fuori vista (es. la sfera-terreno).
	void Application::DrawSelectionOverlay() {
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		const ImU32 accent = m_Selection.IsValid() ? IM_COL32(255, 170, 40, 255) : IM_COL32(180, 180, 180, 200);
		std::string label = GetSelectionLabel();
		DrawTextWithShadow(drawList, ImVec2(m_ViewportPos.x + 14.0f, m_ViewportPos.y + 14.0f), accent, label.c_str());

		if (!m_Selection.IsValid())
			return;

		glm::vec3 boundsMin, boundsMax;
		GetSelectionBounds(boundsMin, boundsMax);

		glm::mat4 viewProjection = m_Camera.GetProjection() * m_Camera.GetView();

		// gli 8 angoli: il bit i-esimo sceglie min/max sull'asse i
		ImVec2 corners[8];
		bool visible[8];
		for (int i = 0; i < 8; i++) {
			glm::vec3 corner((i & 1) ? boundsMax.x : boundsMin.x,
							 (i & 2) ? boundsMax.y : boundsMin.y,
							 (i & 4) ? boundsMax.z : boundsMin.z);
			visible[i] = WorldToScreen(viewProjection, corner, m_ViewportPos, m_ViewportSize, corners[i]);
		}

		// i 12 spigoli collegano angoli che differiscono per un solo bit
		static const int edges[12][2] = {
			{0,1},{0,2},{0,4},{1,3},{1,5},{2,3},
			{2,6},{3,7},{4,5},{4,6},{5,7},{6,7}
		};

		for (const auto& edge : edges) {
			// se un estremo sta dietro la camera lo spigolo andrebbe clippato: lo saltiamo
			if (visible[edge[0]] && visible[edge[1]])
				drawList->AddLine(corners[edge[0]], corners[edge[1]], accent, 1.5f);
		}

		ImVec2 pivot;
		if (WorldToScreen(viewProjection, GetSelectionPivot(), m_ViewportPos, m_ViewportSize, pivot))
			drawList->AddCircleFilled(pivot, 4.0f, accent);
	}

	// 'delta' e' una trasformazione affine world-space: per un punto p, p' = delta * p.
	// Applicare il delta (invece di ricostruire una matrice assoluta) permette di ancorare
	// il gizmo dove vogliamo senza dover reinventare la parametrizzazione di ogni primitiva.
	void Application::ApplyDelta(const glm::mat4& delta) {
		const glm::mat3 linear(delta); // parte lineare: rotazione + scala

		// fattori di scala per asse = lunghezza delle colonne della parte lineare
		const glm::vec3 axisScale(glm::length(linear[0]), glm::length(linear[1]), glm::length(linear[2]));

		switch (m_Selection.Type) {
		case SelectionType::Sphere: {
			Sphere& s = m_World.Spheres.at(m_Selection.Index);
			glm::vec3 center = glm::vec3(delta * glm::vec4(glm::vec3(s.Position), 1.0f));

			// una sfera ha un solo raggio: la scala puo' essere solo uniforme
			float uniformScale = (axisScale.x + axisScale.y + axisScale.z) / 3.0f;
			s.Position = glm::vec4(center, glm::max(s.Position.w * uniformScale, 1e-4f));
			break;
		}
		case SelectionType::Quad: {
			Quad& q = m_World.Quads.at(m_Selection.Index);

			// i lati del quad sono U*Width e V*Height: si trasformano come direzioni
			glm::vec3 edgeU = linear * (glm::vec3(q.U) * q.Width);
			glm::vec3 edgeV = linear * (glm::vec3(q.V) * q.Height);

			float width = glm::length(edgeU);
			float height = glm::length(edgeV);
			if (width < 1e-6f || height < 1e-6f)
				break; // quad degenere: ignoriamo la manipolazione

			q.PositionLLC = glm::vec4(glm::vec3(delta * glm::vec4(glm::vec3(q.PositionLLC), 1.0f)), 0.0f);
			q.Width = width;
			q.Height = height;
			q.U = glm::vec4(edgeU / width, 0.0f);
			q.V = glm::vec4(edgeV / height, 0.0f);
			break;
		}
		case SelectionType::Box: {
			Box& b = m_World.Boxes.at(m_Selection.Index);
			glm::vec3 center = 0.5f * (glm::vec3(b.Min) + glm::vec3(b.Max));
			glm::vec3 size = glm::vec3(b.Max) - glm::vec3(b.Min);

			center = glm::vec3(delta * glm::vec4(center, 1.0f));
			// il box e' axis-aligned: si puo' scalare, non ruotare
			// UpdateBox normalizza gli assi, quindi una dimensione nulla darebbe NaN
			size = glm::max(size * axisScale, glm::vec3(1e-3f));

			b.Min = glm::vec4(center - size * 0.5f, 0.0f);
			b.Max = glm::vec4(center + size * 0.5f, 0.0f);
			b.UpdateBox(m_World.Quads); // riscrive i 6 quad che compongono il box
			break;
		}
		case SelectionType::Mesh: {
			// nessuna ricostruzione del BVH: cambia solo la trasformazione dell'istanza
			const MeshInfo& mesh = m_World.Meshes.at(m_Selection.Index);
			m_World.SetMeshTransform(m_Selection.Index, delta * mesh.Transform);
			break;
		}
		default:
			break;
		}
	}

	// Rete di sicurezza: la selezione e' un indice in una collezione che puo' rimpicciolirsi
	// sotto i piedi (cambio scena e, in futuro, cancellazione di oggetti). Invece di
	// affidarsi al fatto che ogni punto di mutazione si ricordi di azzerarla, la si
	// verifica contro il mondo corrente prima di usarla.
	void Application::ValidateSelection() {
		if (!m_Selection.IsValid())
			return;

		size_t count = 0;
		switch (m_Selection.Type) {
		case SelectionType::Sphere: count = m_World.Spheres.size(); break;
		case SelectionType::Quad:   count = m_World.Quads.size();   break;
		case SelectionType::Box:    count = m_World.Boxes.size();   break;
		case SelectionType::Mesh:   count = m_World.Meshes.size();  break;
		default: break;
		}

		if (static_cast<size_t>(m_Selection.Index) >= count)
			m_Selection = {};
	}

	void Application::SetSelectionFromPick(int objectType, int objectIndex) {
		if (objectType < 0 || objectIndex < 0) {
			m_Selection = {};
			return;
		}

		// lo shader non conosce i box: riporta l'indice del quad colpito, e i 6 quad
		// consecutivi a partire da Box::index appartengono a quel box
		if (objectType == static_cast<int>(SelectionType::Quad)) {
			for (size_t b = 0; b < m_World.Boxes.size(); b++) {
				int firstQuad = static_cast<int>(m_World.Boxes[b].index);
				if (objectIndex >= firstQuad && objectIndex < firstQuad + 6) {
					m_Selection = { SelectionType::Box, static_cast<int>(b) };
					return;
				}
			}
		}

		m_Selection = { static_cast<SelectionType>(objectType), objectIndex };
	}

	void Application::UpdateSelection() {
		ImGuiIO& io = ImGui::GetIO();





		// Le scorciatoie non devono agire mentre si scrive in un widget o si naviga con
		// la camera (tasto destro + WASD). Il test giusto e' WantTextInput: con
		// NavEnableKeyboard attivo, WantCaptureKeyboard resta sempre vero e bloccherebbe
		// ogni scorciatoia.
		// con la modale di conferma aperta, Escape deve chiuderla e non deselezionare,
		// e Canc non deve riaprirla: le scorciatoie restano sospese
		if (!io.WantTextInput && !m_ShowDeletePopup && !Input::IsMouseButtonDown(MouseButton::Right)) {
			if (ImGui::IsKeyPressed(ImGuiKey_G)) m_GizmoOperation = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoOperation = ImGuizmo::ROTATE;
			if (ImGui::IsKeyPressed(ImGuiKey_S)) m_GizmoOperation = ImGuizmo::SCALE;
			if (ImGui::IsKeyPressed(ImGuiKey_Escape)) m_Selection = {};

			if (ImGui::IsKeyPressed(ImGuiKey_Delete) && CanDeleteSelection())
				m_DeleteRequested = true;
		}

		// il gizmo ha la precedenza sul picking: trascinarlo non deve deselezionare.
		// In modalita' fuoco pero' si clicca per mettere a fuoco, anche sopra il gizmo.
		const bool gizmoBusy = m_Selection.IsValid() && (ImGuizmo::IsOver() || ImGuizmo::IsUsing());

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_ViewportHovered && (!gizmoBusy || m_FocusPickArmed)) {
			ImVec2 mouse = ImGui::GetMousePos();

			// coordinate locali al rettangolo dell'immagine, non alla finestra OS
			float localX = mouse.x - m_ViewportPos.x;
			float localY = mouse.y - m_ViewportPos.y;

			if (localX >= 0.0f && localY >= 0.0f && localX < m_ViewportSize.x && localY < m_ViewportSize.y) {
				int px = static_cast<int>(localX);
				// AddImage campiona con V ribaltata (0,1)->(1,0): la riga 0 dell'immagine
				// del compute sta in BASSO, mentre il mouse ha origine in alto
				int py = static_cast<int>(m_ViewportSize.y - 1.0f - localY);

				// il pick e' in volo per un frame: ricorda ORA se e' per il fuoco, prima
				// che il bottone/modalita' cambino
				m_PickForFocus = m_FocusPickArmed;
				m_FocusPickArmed = false;

				m_LastPickPixel = { px, py };
				m_PickRequestCount++;
				m_Renderer.RequestPick(px, py);
			}
		}

		// il risultato e' pronto il frame dopo la richiesta
		int pickedType, pickedIndex;
		float pickedDistance;
		if (m_Renderer.ConsumePickResult(pickedType, pickedIndex, pickedDistance)) {
			m_LastPickType = pickedType;
			m_LastPickIndex = pickedIndex;

			if (m_PickForFocus) {
				m_PickForFocus = false;
				// distanza < 0 = il raggio ha mancato tutto (sfondo): non spostare il fuoco
				if (pickedDistance > 0.0f) {
					m_Renderer.FocusDistance = pickedDistance;
					m_Renderer.ResetPathTracingCounter(); // il fuoco cambia i raggi primari
				}
			}
			else {
				// il pick e' stato richiesto un frame fa: se nel mezzo la scena e' cambiata,
				// l'indice si riferisce a oggetti che non esistono piu'
				SetSelectionFromPick(pickedType, pickedIndex);
				ValidateSelection();

				if (m_Selection.IsValid())
					m_GizmoOperation = ImGuizmo::TRANSLATE; // default alla selezione, come Blender
			}
		}
	}

	void Application::DrawGizmo() {
		if (!m_Selection.IsValid())
			return;

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::AllowAxisFlip(false);
		// draw list della finestra "Viewport": e' li' che vive l'immagine path-traced
		ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
		ImGuizmo::SetRect(m_ViewportPos.x, m_ViewportPos.y, m_ViewportSize.x, m_ViewportSize.y);

		const ImGuizmo::OPERATION operation = EffectiveGizmoOperation();

		// Mentre si trascina, la matrice deve conservare lo stato accumulato da ImGuizmo;
		// appena si rilascia, la si ri-ancora al centro corrente dell'oggetto.
		if (!ImGuizmo::IsUsing())
			m_GizmoMatrix = glm::translate(glm::mat4(1.0f), GetSelectionPivot());

		glm::mat4 view = m_Camera.GetView();
		glm::mat4 projection = m_Camera.GetProjection();
		glm::mat4 delta(1.0f);

		if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection),
								 operation, ImGuizmo::WORLD,
								 glm::value_ptr(m_GizmoMatrix), glm::value_ptr(delta))) {
			ApplyDelta(delta);
			m_Renderer.ResetPathTracingCounter(); // la scena e' cambiata: riparte l'accumulo
		}
	}

	void Application::Render(float deltaTime) {
		m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Renderer.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Renderer.Render(m_Camera, m_World);

		glClear(GL_COLOR_BUFFER_BIT);
	}
}
