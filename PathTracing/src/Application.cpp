#include "stb_image_write.h"
#include "Application.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "imgui_internal.h"
#include "Input.h"
#include "Random.h"

namespace PathTracer {

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
		ImGui::Text("FPS: %f", m_FPS);
		ImGui::Text("Last Render: %.3fms", deltaTime * 1000);
		ImGui::Spacing();
		ImGui::SeparatorText("OpenGL info:");
		ImGui::Text("Vendor: %s", glGetString(GL_VENDOR));
		ImGui::Text("Renderer: %s", glGetString(GL_RENDERER));
		ImGui::Text("Version: %s", glGetString(GL_VERSION));
		ImGui::Separator();
		
		ImGui::Spacing();

		if (ImGui::Button("Screenshot")) {
			m_Renderer.GetFrameBuffer()->SavePPMTexture();
		}

		ImGui::Separator();
		ImGui::Spacing();

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
		ImGui::SeparatorText("ENGINE CONFIGURATIONS");
		ImGui::PopStyleColor();
		ImGui::Spacing();
		if (ImGui::Checkbox("Vsync", &m_Vsync))
			glfwSwapInterval(m_Vsync);
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Checkbox("Post Processing", &m_Renderer.PostProcessing);
		ImGui::DragFloat("HDR", &m_Renderer.Exposure, 0.01f, 0.0f, 10.0f);
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text("Samples Per Pixel:");
		if(ImGui::SliderInt("samples", &m_Renderer.m_SamplesPerPixel, 1, 16))
			m_Renderer.ResetPathTracingCounter();
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text("Ray Depth:");
		if(ImGui::SliderInt("ray", &m_Renderer.m_RayDepth, 1, 50))
			m_Renderer.ResetPathTracingCounter();
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text("Selection:");
		ImGui::Text("%s", GetSelectionLabel().c_str());
		ImGui::TextDisabled("click = seleziona | G/R/S = sposta/ruota/scala | ESC = deseleziona");

		if (ImGui::TreeNode("Picking debug")) {
			ImGui::Text("hovered window : %s", HoveredWindowName());
			ImGui::Text("viewport hover : %s", m_ViewportHovered ? "SI" : "NO");
			ImGui::Text("viewport rect  : (%.0f,%.0f) %.0f x %.0f",
				m_ViewportPos.x, m_ViewportPos.y, m_ViewportSize.x, m_ViewportSize.y);
			ImGui::Text("richieste pick : %d", m_PickRequestCount);
			ImGui::Text("ultimo pixel   : (%d, %d)", m_LastPickPixel.x, m_LastPickPixel.y);
			ImGui::Text("ultimo esito   : type=%d index=%d  (-2=shader non scrive, -1=nessun hit)",
				m_LastPickType, m_LastPickIndex);

			// Bypassa il gate del mouse: se questo seleziona ma il click no, il problema
			// e' nel gate; se non seleziona nemmeno questo, e' nella pipeline di picking.
			if (ImGui::Button("Pick al centro della viewport")) {
				m_LastPickPixel = { (int)m_ViewportSize.x / 2, (int)m_ViewportSize.y / 2 };
				m_PickRequestCount++;
				m_Renderer.RequestPick(m_LastPickPixel.x, m_LastPickPixel.y);
			}

			ImGui::TreePop();
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text("BVH Debugger:");
		if (ImGui::Checkbox("BVH Heatmap", &m_Renderer.BVHDebug))
			m_Renderer.ResetPathTracingCounter();
		if (m_Renderer.BVHDebug) {
			ImGui::TextDisabled("blu = poche ricerche, rosso = molte");
			ImGui::SliderFloat("heat scale", &m_Renderer.BVHHeatScale, 1.0f, 256.0f);
		}
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Checkbox("Enable Path Tracing", &m_Renderer.PathTracing);
		if (ImGui::Button("Reset Accumulation"))
			m_Renderer.ResetPathTracingCounter();

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
		ImGui::SeparatorText("CAMERA CONFIGURATIONS");
		ImGui::PopStyleColor();

		if (ImGui::SliderFloat("Camera FOV (Vertical)", &m_Camera.m_VerticalFOV, 30.0f, 140.0f)) {
			m_Camera.RecalculateProjection();
			m_Renderer.ResetPathTracingCounter();
		}

		if (ImGui::Button("Reset Camera Pos")) {
			m_Camera.ResetPosition();
			m_Renderer.ResetPathTracingCounter();
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
		ImGui::SeparatorText("SCENE CONFIGURATIONS");
		ImGui::PopStyleColor();	


		ImGui::Separator();
		ImGui::Spacing();
		ImGui::Text("Background");
		if(ImGui::Checkbox("Environment Mapping", &m_Renderer.EnvironmentMapping)) {
			m_Renderer.ResetPathTracingCounter();
		}

		if(!m_Renderer.EnvironmentMapping)
			if(ImGui::ColorEdit3("Background Color", glm::value_ptr(m_World.BackgroundColor)))
				m_Renderer.ResetPathTracingCounter();

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text("Select the scene");
		if (ImGui::Combo("Current Scene", &m_World.CurrentScene, "TWO SPHERES\0RANDOM SPHERES\0CORNELL BOX\0RANDOM BOXES\0CORNELL BOW WITH MESHES\0SETUP - 1\0SETUP - 2\0SETUP - 3\0\0")) {
			// gli indici della vecchia scena non significano piu' niente: la finestra
			// "Viewport" viene disegnata piu' avanti in QUESTO stesso frame e userebbe
			// la selezione stale per indicizzare collezioni gia' svuotate
			m_Selection = {};

			m_World.DestroyScene();
			m_Renderer.ResetPathTracingCounter(true);
			m_World.LoadScene();
		}

		ImGui::Spacing();
		ImGui::Spacing();

		ImGui::PushItemWidth(200.0);
		ImGui::Text("Objects:");

		if (m_World.Spheres.size() > 0) {
			if (ImGui::TreeNode("Spheres")) {
				for (size_t i = 0; i < m_World.Spheres.size(); i++) {
					Sphere& sphere = m_World.Spheres.at(i);

					ImGui::PushID(i);
					if (ImGui::DragFloat3("Position", glm::value_ptr(sphere.Position), 0.1f)) m_Renderer.ResetPathTracingCounter();
					if (ImGui::DragFloat("Radius", &sphere.Position.w, 0.1f, -10.0f, 10.0f)) m_Renderer.ResetPathTracingCounter();
					if (ImGui::DragFloat("Material", &sphere.MaterialIndex, 1.0f, 0, (int)m_World.Materials.size() - 1)) m_Renderer.ResetPathTracingCounter();

					ImGui::Separator();
					ImGui::PopID();
				}

				ImGui::TreePop();
			}
		}

		if (m_World.Quads.size() > 0) {
			if (ImGui::TreeNode("Quads")) {
				for (size_t i = 0; i < m_World.Quads.size(); i++) {
					Quad& quad = m_World.Quads.at(i);

					ImGui::PushID(i);
					if (ImGui::DragFloat3("Position", glm::value_ptr(quad.PositionLLC), 0.1f)) m_Renderer.ResetPathTracingCounter();
					if (ImGui::DragFloat3("U", glm::value_ptr(quad.U), 0.1f)) m_Renderer.ResetPathTracingCounter();
					if (ImGui::DragFloat3("V", glm::value_ptr(quad.V), 0.1f)) m_Renderer.ResetPathTracingCounter();
					if (ImGui::DragFloat("Width", &quad.Width, 0.1f)) m_Renderer.ResetPathTracingCounter();
					if (ImGui::DragFloat("Height", &quad.Height, 0.1f)) m_Renderer.ResetPathTracingCounter();
					if (ImGui::DragFloat("Material", &quad.MaterialIndex, 1.0f, 0, (int)m_World.Materials.size() - 1)) m_Renderer.ResetPathTracingCounter();
					ImGui::Separator();
					ImGui::PopID();
				}

				ImGui::TreePop();
			}
		}


		if (m_World.Boxes.size() > 0) {
			if (ImGui::TreeNode("Boxes")) {
				for (size_t i = 0; i < m_World.Boxes.size(); i++) {
					Box& box = m_World.Boxes.at(i);

					ImGui::PushID(i);
					if (ImGui::DragFloat3("Min", glm::value_ptr(box.Min), 0.1f)) { m_Renderer.ResetPathTracingCounter(); box.UpdateBox(m_World.Quads); }
					if (ImGui::DragFloat3("Max", glm::value_ptr(box.Max), 0.1f)) { m_Renderer.ResetPathTracingCounter(); box.UpdateBox(m_World.Quads); }
					if (ImGui::DragFloat("Material", &box.MaterialIndex, 1.0f, 0, (int)m_World.Materials.size() - 1)) { m_Renderer.ResetPathTracingCounter(); box.UpdateBox(m_World.Quads); }
					ImGui::Separator();
					ImGui::PopID();
				}

				ImGui::TreePop();
			}
		}

		if (m_World.Meshes.size() > 0) {
			if (ImGui::TreeNode("Meshes")) {
				for (size_t i = 0; i < m_World.Meshes.size(); i++) {
					MeshInfo& mesh = m_World.Meshes.at(i);

					ImGui::PushID(i);
					// la traslazione vive nell'ultima colonna della Transform
					glm::vec3 position = glm::vec3(mesh.Transform[3]);
					if (ImGui::DragFloat3("Position", glm::value_ptr(position), 0.1f)) {
						glm::mat4 transform = mesh.Transform;
						transform[3] = glm::vec4(position, 1.0f);
						m_World.SetMeshTransform((int)i, transform);
						m_Renderer.ResetPathTracingCounter();
					}
					if (ImGui::DragFloat("Material", &mesh.MaterialIndex, 1.0f, 0, (int)m_World.Materials.size() - 1)) { m_Renderer.ResetPathTracingCounter(); }
					ImGui::Separator();
					ImGui::PopID();
				}

				ImGui::TreePop();
			}
		}

		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text("Materials List:");

		if(ImGui::TreeNode("Materials") && m_World.Materials.size() > 0) {
			ImGui::Checkbox("Linked Colors", &m_LinkColors);
			ImGui::Spacing();

			for(size_t i = 0; i < m_World.Materials.size(); i++) {
				Material& material = m_World.Materials.at(i);

				ImGui::PushID(i);

				if(ImGui::TreeNode(("Material " + std::to_string(i)).c_str())) {
					if (ImGui::ColorEdit3("Color", glm::value_ptr(material.Color))) {
						if(m_LinkColors) {
							material.RefractionColor = material.Color;
							material.SpecularColor = material.Color;
						}

						m_Renderer.ResetPathTracingCounter();
					}
					if(!m_LinkColors) {
						if (ImGui::ColorEdit3("Specular Color", glm::value_ptr(material.SpecularColor))) m_Renderer.ResetPathTracingCounter();
						if (ImGui::ColorEdit3("Refraction Color", glm::value_ptr(material.RefractionColor))) m_Renderer.ResetPathTracingCounter();
					}

					if (ImGui::DragFloat("Roughness", &material.Roughness, 0.05f, 0.0f, 1.0f)) m_Renderer.ResetPathTracingCounter();
					if (ImGui::DragFloat("Specular Probability", &material.SpecularProbability, 0.05, 0.0f, 1.0f)) m_Renderer.ResetPathTracingCounter();
					if (ImGui::DragFloat("Refraction Index", &material.RefractionRatio, 0.01f, 1.0f, 3.0f)) m_Renderer.ResetPathTracingCounter();
					if (ImGui::DragFloat("Refractin Probability", &material.RefractionProbability, 0.05, 0.0f, 1.0f)) m_Renderer.ResetPathTracingCounter();
					if (ImGui::DragFloat("Refractin Roughness", &material.RefractionRoughness, 0.05, 0.0f, 1.0f)) m_Renderer.ResetPathTracingCounter();
					if (ImGui::DragFloat("Emissive Strenght", &material.EmissiveStrenght, 0.1f, 0.0f, FLT_MAX)) m_Renderer.ResetPathTracingCounter();
					if (ImGui::ColorEdit3("Emissive Color", glm::value_ptr(material.EmissiveColor))) m_Renderer.ResetPathTracingCounter();

					ImGui::Separator();
					int checkerMode = (int)(material.Checker + 0.5f);
					if (ImGui::Combo("Checker", &checkerMode, "Off\0UV (debug)\0World space\0\0")) {
						material.Checker = (float)checkerMode;
						m_Renderer.ResetPathTracingCounter();
					}
					if (checkerMode != 0) {
						if (ImGui::DragFloat("Checker scale", &material.CheckerScale, 0.1f, 0.01f, 64.0f))
							m_Renderer.ResetPathTracingCounter();
					}

					// -1 = nessuna texture; gli altri valori indicizzano i layer di TexturePaths.
					// Con min == max ImGui disattiva il clamping, quindi senza texture
					// il drag va nascosto del tutto invece che lasciato libero di sforare.
					int layerCount = (int)m_World.TexturePaths.size();
					if (layerCount > 0) {
						if (ImGui::DragFloat("Albedo texture layer", &material.AlbedoTexture, 1.0f, -1.0f, (float)(layerCount - 1)))
							m_Renderer.ResetPathTracingCounter();
					}
					else {
						material.AlbedoTexture = -1.0f;
						ImGui::TextDisabled("(nessuna texture caricata in questa scena)");
					}

					if (checkerMode != 0 && material.SpecularProbability > 0.5f)
						ImGui::TextDisabled("Specular Probability alta: l'albedo si vede poco");

					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			ImGui::TreePop();
		}

		ImGui::PopItemWidth();

		ImGui::End();

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

	std::string Application::GetSelectionLabel() const {
		if (!m_Selection.IsValid())
			return "Nessuna selezione  -  click su un oggetto per selezionarlo";

		const ImGuizmo::OPERATION effective = EffectiveGizmoOperation();
		const char* operation = effective == ImGuizmo::ROTATE ? "Rotate"
							  : effective == ImGuizmo::SCALE  ? "Scale" : "Translate";

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
		default: name = "?"; break;
		}

		std::string label = name + " #" + std::to_string(m_Selection.Index) + "  -  " + operation;
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
		if (!io.WantTextInput && !Input::IsMouseButtonDown(MouseButton::Right)) {
			if (ImGui::IsKeyPressed(ImGuiKey_G)) m_GizmoOperation = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoOperation = ImGuizmo::ROTATE;
			if (ImGui::IsKeyPressed(ImGuiKey_S)) m_GizmoOperation = ImGuizmo::SCALE;
			if (ImGui::IsKeyPressed(ImGuiKey_Escape)) m_Selection = {};
		}

		// il gizmo ha la precedenza sul picking: trascinarlo non deve deselezionare
		const bool gizmoBusy = m_Selection.IsValid() && (ImGuizmo::IsOver() || ImGuizmo::IsUsing());

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_ViewportHovered && !gizmoBusy) {
			ImVec2 mouse = ImGui::GetMousePos();

			// coordinate locali al rettangolo dell'immagine, non alla finestra OS
			float localX = mouse.x - m_ViewportPos.x;
			float localY = mouse.y - m_ViewportPos.y;

			if (localX >= 0.0f && localY >= 0.0f && localX < m_ViewportSize.x && localY < m_ViewportSize.y) {
				int px = static_cast<int>(localX);
				// AddImage campiona con V ribaltata (0,1)->(1,0): la riga 0 dell'immagine
				// del compute sta in BASSO, mentre il mouse ha origine in alto
				int py = static_cast<int>(m_ViewportSize.y - 1.0f - localY);

				m_LastPickPixel = { px, py };
				m_PickRequestCount++;
				m_Renderer.RequestPick(px, py);
			}
		}

		// il risultato e' pronto il frame dopo la richiesta
		int pickedType, pickedIndex;
		if (m_Renderer.ConsumePickResult(pickedType, pickedIndex)) {
			m_LastPickType = pickedType;
			m_LastPickIndex = pickedIndex;

			// il pick e' stato richiesto un frame fa: se nel mezzo la scena e' cambiata,
			// l'indice si riferisce a oggetti che non esistono piu'
			SetSelectionFromPick(pickedType, pickedIndex);
			ValidateSelection();

			if (m_Selection.IsValid())
				m_GizmoOperation = ImGuizmo::TRANSLATE; // default alla selezione, come Blender
		}
	}

	void Application::DrawGizmo() {
		if (!m_Selection.IsValid())
			return;

		ImGuizmo::SetOrthographic(false);
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
