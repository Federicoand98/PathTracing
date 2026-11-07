#include "stb/stb_image_write.h"
#include "Editor/Application.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "imgui_internal.h"
#include "Editor/Input.h"
#include "Core/Random.h"
#include "Serialization/SceneSerializer.h"
#include "Import/Importer.h"
#include "vendor/portable-file-dialogs.h"

namespace Lucerna {

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
			return hovered ? hovered->Name : "(none)";
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

		// Test/CI headless: PT_SCENE=<n> sceglie la scena prima del load.
		if (const char* sc = getenv("PT_SCENE"))
			m_World.CurrentScene = atoi(sc);

		m_World.LoadScene();

		// Test/CI headless: PT_GLTF=<file> apre una scena glTF al posto di quella di default.
		if (const char* g = getenv("PT_GLTF"))
			OpenFile(g); // qualunque formato del registry, non solo glTF
		// PT_SCENE_FILE=<file.json> apre una scena salvata (formato 2)
		if (const char* f = getenv("PT_SCENE_FILE"))
			SceneSerializer(m_World, &m_Renderer.Settings).Deserialize(f);

		// Test/CI headless: PT_NEE=0 forza la NEE spenta (A/B col light sampling).
		// PT_CAM=x,y,z,dx,dy,dz: posizione e direzione della camera, per catture ripetibili
		if (const char* c = getenv("PT_CAM")) {
			glm::vec3 pos, dir;
			if (sscanf(c, "%f,%f,%f,%f,%f,%f", &pos.x, &pos.y, &pos.z, &dir.x, &dir.y, &dir.z) == 6) {
				m_World.MoveCamera(pos, glm::normalize(dir)); // e' l'inquadratura: la vista la segue in SyncCameras
			}
		}

		if (const char* a = getenv("PT_AOV"))
			m_Renderer.View.AOVView = atoi(a);

		if (const char* n = getenv("PT_NEE"))
			m_Renderer.Settings.NEE = (atoi(n) != 0);
		// Test/CI headless: PT_DEPTH=<n> forza la profondita' dei rimbalzi (2 = solo diretta).
		if (const char* d = getenv("PT_DEPTH"))
			m_Renderer.Settings.RayDepth = atoi(d);
		// Test/CI headless: PT_REPROJ=0 spegne la riproiezione temporale (misura del suo costo).
		if (const char* r = getenv("PT_REPROJ"))
			m_Renderer.Settings.Reproject = (atoi(r) != 0);
		// Test/CI headless: PT_DENOISE=1 accende il denoiser (misura del suo costo).
		if (const char* dn = getenv("PT_DENOISE"))
			m_Renderer.Settings.Denoise = (atoi(dn) != 0);
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

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		m_Window = glfwCreateWindow(width, height, "Lucerna", nullptr, nullptr);

		if (m_Window == nullptr)
			return false;

		// Test/CI headless: PT_NOVSYNC=1. Con la vsync il driver abbassa i clock per riempire il
		// frame, quindi i tempi GPU non misurano il costo reale delle passate.
		m_Preferences = Preferences::Load();
		m_Vsync = m_Preferences.Vsync && getenv("PT_NOVSYNC") == nullptr;
		if (!m_Vulkan.Initialize(m_Window, m_Vsync))
			return false;

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

		ImGui_ImplGlfw_InitForVulkan(m_Window, true);

		// Le finestre staccate di ImGui hanno ognuna la sua swapchain e i suoi set: da qui la
		// dimensione generosa del pool.
		VkDescriptorPoolSize poolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 };
		VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.maxSets = 256;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		vkCreateDescriptorPool(m_Vulkan.Device(), &poolInfo, nullptr, &m_ImGuiPool);

		VkFormat swapchainFormat = m_Vulkan.SwapchainFormat();
		ImGui_ImplVulkan_InitInfo initInfo{};
		initInfo.Instance = m_Vulkan.Instance();
		initInfo.PhysicalDevice = m_Vulkan.PhysicalDevice();
		initInfo.Device = m_Vulkan.Device();
		initInfo.QueueFamily = m_Vulkan.QueueFamily();
		initInfo.Queue = m_Vulkan.Queue();
		initInfo.DescriptorPool = m_ImGuiPool;
		initInfo.MinImageCount = m_Vulkan.SwapchainImageCount();
		initInfo.ImageCount = m_Vulkan.SwapchainImageCount();
		initInfo.UseDynamicRendering = true;
		initInfo.ColorAttachmentFormat = swapchainFormat;
		ImGui_ImplVulkan_Init(&initInfo, VK_NULL_HANDLE);

		m_Vulkan.Immediate([](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });
		ImGui_ImplVulkan_DestroyFontUploadObjects();

		return true;
	}

	void Application::RunLoop() {
		ImGuiIO& io = ImGui::GetIO();

		m_Camera.OnResize(m_Width, m_Height);
		m_Renderer.OnResize(m_Width, m_Height);
		m_Renderer.Initialize();

		while (!glfwWindowShouldClose(m_Window) && m_IsRunning) {
			glfwPollEvents();

			if (!m_Vulkan.BeginFrame())
				continue;

			// La camera che si muove NON azzera piu' l'accumulo: la riproiezione temporale
			// riusa il frame precedente invece di buttarlo (ADR 0002). Il reset resta per i
			// cambi di scena/materiale, dove la storia e' genuinamente invalida.
			// SetInteracting dice al renderer che la storia va cappata (ghosting) e che puo'
			// abbassare la risoluzione (render scale).
			bool cameraMoved = m_Camera.OnUpdate(m_DeltaTime);
			SyncCameras(cameraMoved);
			m_Renderer.SetInteracting(cameraMoved);

			// Live-link glTF: ricarica la scena se il file e' stato ri-esportato da Blender.
			PollLiveLink();

			// Start the Dear ImGui frame
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();

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
					ImGuiID dockspace_id = ImGui::GetID("OpenGL Dockspace"); // il nome resta: e' la chiave del layout salvato in imgui.ini
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

			m_Vulkan.BeginUiPass();
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_Vulkan.Cmd());
			m_Vulkan.EndUiPass();
			m_Vulkan.SubmitFrame();

			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}

			m_Vulkan.PresentFrame();

			CalculateTime();
			HandleCaptures();
		}
	}

	// Va dopo la submit: le immagini si leggono solo a frame eseguito.
	void Application::HandleCaptures() {
		if (m_ScreenshotRequested) {
			m_Renderer.SaveScreenshot();
			m_ScreenshotRequested = false;
		}

		// Cattura headless: PT_SHOT=<file> PT_SHOT_FRAME=<n> salva dopo n frame accumulati
		// e chiude. Serve a ispezionare il render senza guardare lo schermo (test, CI).
		if (const char* shot = getenv("PT_SHOT")) {
			static int shotFrame = getenv("PT_SHOT_FRAME") ? atoi(getenv("PT_SHOT_FRAME")) : 200;
			static int n = 0;
			if (++n >= shotFrame) {
				m_Renderer.SaveScreenshot(shot);
				// PT_HDR=<file>: dump della beauty LINEARE (rgba32f, pre-tonemap) come raw
				// float32. Serve a confronti non falsati da tonemap/clipping a 8 bit.
				if (const char* hdr = getenv("PT_HDR"))
					m_Renderer.SaveHdr(hdr);
				m_IsRunning = false;
			}
		}
	}

	// Apre un file, di qualunque formato il registry conosca (issue #27), e lo registra per il
	// live-link. Se il file non si legge la scena resta intatta e il watch non si arma. Al
	// successo salva path e mtime: da qui in poi PollLiveLink lo sorveglia.
	bool Application::OpenFile(const std::string& path) {
		m_Selection = {};
		if (!m_World.Open(path))
			return false;

		m_WatchPath = path;
		std::error_code ec;
		m_WatchMtime = std::filesystem::last_write_time(path, ec); // ec ignorato: se fallisce, 0 -> ricarica al primo poll
		return true;
	}

	// Il filtro del file dialog: le estensioni di tutti gli importer, poi un formato per riga
	std::vector<std::string> Application::FileFilters() {
		std::vector<std::string> filters;
		std::string all;
		for (const auto& importer : Importers()) {
			std::string patterns;
			for (const std::string& e : importer->Extensions())
				patterns += (patterns.empty() ? "*" : " *") + e;
			all += (all.empty() ? "" : " ") + patterns;
			filters.push_back(importer->Name());
			filters.push_back(patterns);
		}
		filters.insert(filters.begin(), { "Supported files", all });
		filters.push_back("All files");
		filters.push_back("*");
		return filters;
	}

	// Live-link: se il file aperto e' cambiato (Blender l'ha ri-esportato su save), lo riapre.
	// Chiamato ogni frame; il costo a regime e' una sola stat() del file.
	void Application::PollLiveLink() {
		if (!m_LiveLink || m_WatchPath.empty())
			return;

		std::error_code ec;
		auto mtime = std::filesystem::last_write_time(m_WatchPath, ec);
		if (ec || mtime == m_WatchMtime)
			return; // file assente/in scrittura, o invariato

		// Aggiorna il timestamp PRIMA di ricaricare: se il parse fallisce (export a meta',
		// file troncato) Open lascia la scena com'e' e non ri-tenta all'infinito sullo stesso
		// timestamp; ci riprovera' al prossimo cambio di mtime (Blender riscrive).
		m_WatchMtime = mtime;
		std::cout << "Live-link: rilevato cambio, riapro " << m_WatchPath << std::endl;
		m_Selection = {};
		m_World.Open(m_WatchPath);
	}

	void Application::CalculateTime() {
		float time = glfwGetTime();
		m_Timer = time - m_ResetTimer;
		m_DeltaTime = time - m_LastFrameTime;
		m_NFrames++;

		if (m_Timer >= 1.0) {
			m_FPS = (double)m_NFrames / m_Timer;
			if (getenv("PT_LOG_FPS")) std::cout << "PTFPS " << m_FPS << " pt_ms " << m_Renderer.GetPathTraceMs()
			                                    << " dn_ms " << m_Renderer.GetDenoiseMs() << std::endl; // TEMP: misura headless
			m_NFrames = 0;
			m_ResetTimer = time;
		}
		
		m_LastFrameTime = time;
	}

	void Application::Shutdown() {
		if (m_Vulkan.Device() != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(m_Vulkan.Device());
			if (m_DisplaySet != VK_NULL_HANDLE)
				ImGui_ImplVulkan_RemoveTexture(m_DisplaySet);
			m_Renderer.Release();
			ImGui_ImplVulkan_Shutdown();
			ImGui_ImplGlfw_Shutdown();
			ImGui::DestroyContext();
			vkDestroyDescriptorPool(m_Vulkan.Device(), m_ImGuiPool, nullptr);
		}
		m_Vulkan.Shutdown();

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
		ImGui::SameLine();
		ImGui::TextDisabled("| GPU PT %.2f DN %.2f ms", m_Renderer.GetPathTraceMs(), m_Renderer.GetDenoiseMs());
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80.0f);
		if (ImGui::Button("Screenshot"))
			m_ScreenshotRequested = true;

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
		ImTextureID image = DisplayTexture();
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
			"Two Spheres\0Random Spheres\0Cornell Box\0Random Boxes\0Cornell Box (meshes)\0Setup 1\0Setup 2\0Setup 3\0Sponza\0\0")) {
			// gli indici della vecchia scena non significano piu' niente: la finestra
			// "Viewport" viene disegnata piu' avanti in QUESTO stesso frame e userebbe
			// la selezione stale per indicizzare collezioni gia' svuotate
			m_Selection = {};

			m_World.DestroyScene();
			m_World.LoadScene(); // la scena imposta anche la sua inquadratura
		}

		// Open (issue #27): rimpiazza la scena col contenuto di un file, in qualunque formato il
		// registry conosca. Il path si puo' incollare o scegliere col file dialog nativo ("...").
		static char openPath[512] = "models/gltf_test/BoxTextured/BoxTextured.gltf";
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
		ImGui::InputText("##openpath", openPath, sizeof(openPath));
		ImGui::SameLine();
		if (ImGui::Button("...")) {
			// Dialog nativo (zenity su Linux). Parte dalla cartella del path corrente, se valida.
			const std::string start = std::filesystem::path(openPath).parent_path().string();
			const auto selection = pfd::open_file("Open", start, FileFilters()).result();
			if (!selection.empty()) {
				std::snprintf(openPath, sizeof(openPath), "%s", selection[0].c_str());
				OpenFile(openPath); // la camera del file, se c'e', diventa l'inquadratura
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Open"))
			OpenFile(openPath);
		if (!m_WatchPath.empty()) {
			ImGui::SameLine();
			ImGui::Checkbox("Live-link", &m_LiveLink);
			ImGui::TextDisabled("watch: %s", m_WatchPath.c_str());
		}

		{
			bool environment = m_World.EnvironmentMapping();
			glm::vec3 background = m_World.BackgroundColor();
			bool changed = ImGui::Checkbox("Environment Mapping", &environment);
			if (!environment)
				changed |= ImGui::ColorEdit3("Background", glm::value_ptr(background));
			if (changed)
				m_World.SetBackground(background, environment);
		}

		ImGui::SeparatorText("Sun");
		{
			SunParams sun = m_World.Sun();
			bool changed = ImGui::Checkbox("Sun enabled", &sun.Enabled);
			if (sun.Enabled) {
				changed |= ImGui::SliderFloat("Elevation", &sun.Elevation, -90.0f, 90.0f, "%.1f deg");
				changed |= ImGui::SliderFloat("Azimuth", &sun.Azimuth, 0.0f, 360.0f, "%.1f deg");
				changed |= ImGui::SliderFloat("Angular radius", &sun.AngularRadius, 0.01f, 5.0f, "%.2f deg", ImGuiSliderFlags_Logarithmic);
				changed |= ImGui::ColorEdit3("Sun color", glm::value_ptr(sun.Color));
				changed |= ImGui::DragFloat("Irradiance", &sun.Irradiance, 0.05f, 0.0f, 1000.0f, "%.2f");
			}
			if (changed)
				m_World.SetSun(sun);
		}

		ImGui::SeparatorText("Scene file");
		ImGui::PushItemWidth(200.0f);
		ImGui::InputText("Name", m_SceneFilename, sizeof(m_SceneFilename));
		ImGui::PopItemWidth();

		const std::string scenePath = std::string("scenes/") + m_SceneFilename + ".json";
		if (ImGui::Button("Save")) {
			std::filesystem::create_directories("scenes");
			SceneSerializer(m_World, &m_Renderer.Settings).Serialize(scenePath);
		}
		ImGui::SameLine();
		if (ImGui::Button("Load")) {
			// la scena caricata si segna tutta cambiata: il renderer la ricarica come un cambio scena
			if (SceneSerializer(m_World, &m_Renderer.Settings).Deserialize(scenePath))
				m_Selection = {};
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%s", scenePath.c_str());

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
		std::vector<bool> quadOwnedByBox(m_World.Quads().size(), false);
		for (const Box& box : m_World.Boxes()) {
			int first = static_cast<int>(box.index);
			for (int q = first; q < first + 6 && q < (int)m_World.Quads().size(); q++)
				quadOwnedByBox[q] = true;
		}

		const size_t standaloneQuads = std::count(quadOwnedByBox.begin(), quadOwnedByBox.end(), false);
		const bool empty = m_World.Spheres().empty() && m_World.Boxes().empty()
						&& standaloneQuads == 0 && m_World.Meshes().empty() && m_World.PunctualLights().empty();

		ImGui::BeginChild("OutlinerList", ImVec2(0.0f, 190.0f), true);

		if (empty)
			ImGui::TextDisabled("empty scene");

		if (!m_World.Spheres().empty() && ImGui::TreeNodeEx("spheres", ImGuiTreeNodeFlags_DefaultOpen, "Spheres (%zu)", m_World.Spheres().size())) {
			for (size_t i = 0; i < m_World.Spheres().size(); i++)
				SelectableObjectRow(m_World.Spheres()[i].Name.c_str(), SelectionType::Sphere, (int)i, m_World.Materials().IndexOf(m_World.Spheres()[i].Material));
			ImGui::TreePop();
		}

		if (!m_World.Boxes().empty() && ImGui::TreeNodeEx("boxes", ImGuiTreeNodeFlags_DefaultOpen, "Boxes (%zu)", m_World.Boxes().size())) {
			for (size_t i = 0; i < m_World.Boxes().size(); i++)
				SelectableObjectRow(m_World.Boxes()[i].Name.c_str(), SelectionType::Box, (int)i, m_World.Materials().IndexOf(m_World.Boxes()[i].Material));
			ImGui::TreePop();
		}

		if (standaloneQuads > 0 && ImGui::TreeNodeEx("quads", ImGuiTreeNodeFlags_DefaultOpen, "Quads (%zu)", standaloneQuads)) {
			for (size_t i = 0; i < m_World.Quads().size(); i++) {
				if (quadOwnedByBox[i])
					continue; // faccia di un box

				SelectableObjectRow(m_World.Quads()[i].Name.c_str(), SelectionType::Quad, (int)i, m_World.Materials().IndexOf(m_World.Quads()[i].Material));
			}
			ImGui::TreePop();
		}

		if (!m_World.Meshes().empty() && ImGui::TreeNodeEx("meshes", ImGuiTreeNodeFlags_DefaultOpen, "Meshes (%zu)", m_World.Meshes().size())) {
			for (size_t i = 0; i < m_World.Meshes().size(); i++) {
				SelectableObjectRow(m_World.Meshes()[i].Name.c_str(), SelectionType::Mesh, (int)i, m_World.Materials().IndexOf(m_World.Meshes()[i].Material));
			}
			ImGui::TreePop();
		}

		if (!m_World.PunctualLights().empty() && ImGui::TreeNodeEx("lights", ImGuiTreeNodeFlags_DefaultOpen, "Lights (%zu)", m_World.PunctualLights().size())) {
			for (size_t i = 0; i < m_World.PunctualLights().size(); i++)
				SelectableObjectRow(m_World.PunctualLights()[i].Name.c_str(), SelectionType::Light, (int)i, -1);
			ImGui::TreePop();
		}

		ImGui::EndChild();
	}

	// Una riga dell'outliner: cliccarla equivale a cliccare l'oggetto nella viewport,
	// cosi' selezione, gizmo e contorno restano un'unica nozione.
	bool Application::SelectableObjectRow(const char* label, SelectionType type, int index, int materialIndex) {
		ImGui::PushID(static_cast<int>(type) * 100000 + index);

		const bool selected = (m_Selection.Type == type && SelectedIndex() == index);

		char row[160];
		snprintf(row, sizeof(row), "%s", label);

		const bool clicked = ImGui::Selectable(row, selected);

		if (materialIndex >= 0) { // le luci non hanno materiale
			ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30.0f);
			ImGui::TextDisabled("m%d", materialIndex);
		}

		if (clicked) {
			m_Selection = Select(type, index);
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
			ImGui::TextDisabled("No object selected.");
			ImGui::TextDisabled("Click in the viewport, or on an outliner row.");
			return;
		}

		ImGui::TextColored(ImVec4(1.0f, 0.67f, 0.16f, 1.0f), "%s", GetSelectionLabel().c_str());
		DrawGizmoToolbar();
		ImGui::Spacing();

		ImGui::PushItemWidth(200.0f);

		// il nome si rinomina qui; e' unico nella lista del suo tipo (issue #23)
		{
			NameField(m_Selection.Id, ObjectName(m_Selection));
		}

		switch (m_Selection.Type) {
		case SelectionType::Sphere: {
			Sphere sphere = m_World.Spheres().at(SelectedIndex());
			bool changed = ImGui::DragFloat3("Center", glm::value_ptr(sphere.Position), 0.1f);
			changed |= ImGui::DragFloat("Radius", &sphere.Position.w, 0.05f, 0.001f, 1000.0f);
			changed |= MaterialCombo("Material", sphere.Material);
			if (changed)
				m_World.Set(m_Selection.Id, sphere);
			break;
		}
		case SelectionType::Quad: {
			Quad quad = m_World.Quads().at(SelectedIndex());
			bool changed = ImGui::DragFloat3("Corner", glm::value_ptr(quad.PositionLLC), 0.1f);
			changed |= ImGui::DragFloat3("U axis", glm::value_ptr(quad.U), 0.05f);
			changed |= ImGui::DragFloat3("V axis", glm::value_ptr(quad.V), 0.05f);
			changed |= ImGui::DragFloat("Width", &quad.Width, 0.1f, 0.001f, 1000.0f);
			changed |= ImGui::DragFloat("Height", &quad.Height, 0.1f, 0.001f, 1000.0f);
			changed |= MaterialCombo("Material", quad.Material);
			if (changed)
				m_World.Set(m_Selection.Id, quad);
			ImGui::TextDisabled("pivot = lower-left corner");
			break;
		}
		case SelectionType::Box: {
			Box box = m_World.Boxes().at(SelectedIndex());

			bool changed = false;
			changed |= ImGui::DragFloat3("Min", glm::value_ptr(box.Min), 0.1f);
			changed |= ImGui::DragFloat3("Max", glm::value_ptr(box.Max), 0.1f);
			changed |= MaterialCombo("Material", box.Material);

			if (changed)
				m_World.Set(m_Selection.Id, box); // riscrive anche i suoi 6 quad
			ImGui::TextDisabled("axis-aligned: cannot be rotated");
			break;
		}
		case SelectionType::Mesh: {
			Mesh mesh = m_World.Meshes().at(SelectedIndex());
			const MeshAsset* asset = m_World.AssetOf(mesh);
			ImGui::Text("Triangles: %d", asset ? static_cast<int>(asset->Triangles.size()) : 0);
			// le istanze dello stesso asset dividono triangoli e BLAS (issue #26)
			const int assetIndex = m_World.Assets().Meshes.IndexOf(mesh.Asset);
			ImGui::TextDisabled("mesh asset: %s", assetIndex >= 0 ? m_World.Assets().Meshes[assetIndex].Name.c_str() : "missing");

			glm::vec3 position = glm::vec3(mesh.Transform[3]); // la traslazione e' l'ultima colonna
			bool changed = ImGui::DragFloat3("Position", glm::value_ptr(position), 0.1f);
			if (changed)
				mesh.Transform[3] = glm::vec4(position, 1.0f);
			changed |= MaterialCombo("Material", mesh.Material);
			if (ImGui::Button("Reset transform")) {
				mesh.Transform = glm::mat4(1.0f);
				changed = true;
			}
			if (changed)
				m_World.Set(m_Selection.Id, mesh);
			break;
		}
		case SelectionType::Light: {
			PunctualLight light = m_World.PunctualLights().at(SelectedIndex());
			bool changed = false;
			int type = light.Spot ? 1 : 0;
			if (ImGui::Combo("Type", &type, "Point\0Spot\0\0")) {
				light.Spot = type == 1;
				changed = true;
			}
			changed |= ImGui::DragFloat3("Position", glm::value_ptr(light.Position), 0.05f);
			changed |= ImGui::ColorEdit3("Color", glm::value_ptr(light.Color));
			changed |= ImGui::DragFloat("Intensity", &light.Intensity, 0.1f, 0.0f, 1e6f, "%.2f W/sr", ImGuiSliderFlags_Logarithmic);
			ImGui::TextDisabled("power %.1f W, as a Blender lamp", light.Intensity * 4.0f * 3.14159265f);
			if (light.Spot) {
				if (ImGui::DragFloat3("Direction", glm::value_ptr(light.Direction), 0.01f)) {
					if (glm::length(light.Direction) < 1e-4f)
						light.Direction = { 0.0f, -1.0f, 0.0f };
					light.Direction = glm::normalize(light.Direction);
					changed = true;
				}
				changed |= ImGui::SliderFloat("Outer angle", &light.OuterAngle, 0.5f, 90.0f, "%.1f deg");
				changed |= ImGui::SliderFloat("Inner angle", &light.InnerAngle, 0.0f, light.OuterAngle, "%.1f deg");
				light.InnerAngle = glm::min(light.InnerAngle, light.OuterAngle);
			}
			if (changed)
				m_World.Set(m_Selection.Id, light);
			break;
		}
		default:
			break;
		}

		ImGui::PopItemWidth();

		ImGui::Spacing();
		if (ImGui::Button("Deselect (ESC)"))
			m_Selection = {};

		ImGui::SameLine();
		if (ImGui::Button("Delete (Del)"))
			m_DeleteRequested = true;
	}

	// Un DragFloat sull'indice del materiale non dice niente su cosa si sta scegliendo:
	// qui ogni voce mostra la pastiglia col colore del materiale.
	bool Application::MaterialCombo(const char* label, ObjectId& material) {
		if (m_World.Materials().empty())
			return false;

		const int count = static_cast<int>(m_World.Materials().size());
		const int current = m_World.Materials().IndexOf(material);

		const std::string preview = current >= 0 ? m_World.Materials()[current].Name : "(none)";
		bool changed = false;

		if (ImGui::BeginCombo(label, preview.c_str())) {
			for (int i = 0; i < count; i++) {
				ImGui::PushID(i);

				const glm::vec4& color = m_World.Materials()[i].Color;
				ImGui::ColorButton("##swatch", ImVec4(color.r, color.g, color.b, 1.0f),
					ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(14.0f, 14.0f));
				ImGui::SameLine();

				const std::string& item = m_World.Materials()[i].Name;
				if (ImGui::Selectable(item.c_str(), i == current)) {
					material = m_World.Materials()[i].Id;
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
		if (ImGui::SmallButton("Edit") && current >= 0)
			OpenMaterialEditor(current);
		ImGui::PopID();

		return changed;
	}

	// Le texture di un tipo, piu' "None". Un materiale cita la texture per id (issue #26).
	bool Application::TextureCombo(const char* label, ObjectId& texture, TextureKind kind) {
		const int current = m_World.Textures().IndexOf(texture);
		bool changed = false;
		if (ImGui::BeginCombo(label, current >= 0 ? m_World.Textures()[current].Name.c_str() : "None")) {
			if (ImGui::Selectable("None", current < 0)) {
				texture = {};
				changed = true;
			}
			for (int i = 0; i < static_cast<int>(m_World.Textures().size()); i++) {
				const auto& t = m_World.Textures()[i];
				if (t.Kind != kind)
					continue;
				ImGui::PushID(i);
				if (ImGui::Selectable(t.Name.c_str(), i == current)) {
					texture = t.Id;
					changed = true;
				}
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	// Nuovi oggetti nascono davanti alla camera, non nell'origine: altrimenti finirebbero
	// fuori vista e sembrerebbe che il pulsante non abbia fatto niente.
	glm::vec3 Application::SpawnPosition() const {
		return m_Camera.GetPosition() + m_Camera.GetDirection() * 4.0f;
	}

	// Il materiale scelto per i nuovi oggetti, se esiste ancora; altrimenti il primo, creandolo
	// se la scena non ne ha.
	ObjectId Application::EnsureMaterialExists() {
		if (m_World.Materials().empty())
			m_World.AddMaterial(CreateDefaultPrincipled());
		if (m_World.Materials().IndexOf(m_NewObjectMaterial) < 0)
			m_NewObjectMaterial = m_World.Materials()[0].Id;
		return m_NewObjectMaterial;
	}

	void Application::DrawAddObjectMenu() {
		const glm::vec3 spawn = SpawnPosition();

		if (ImGui::Button("+ Sphere")) {
			const ObjectId material = EnsureMaterialExists();

			Sphere sphere;
			sphere.Position = glm::vec4(spawn, 0.5f); // la w e' il raggio
			sphere.Material = material;
			m_Selection = Select(SelectionType::Sphere, m_World.AddSphere(sphere));
			m_GizmoOperation = ImGuizmo::TRANSLATE;
		}

		ImGui::SameLine();
		if (ImGui::Button("+ Box")) {
			const ObjectId material = EnsureMaterialExists();

			// CreateBox appende i suoi 6 quad in coda, quindi i Box::index gia' esistenti
			// restano validi e in ordine crescente, come si aspetta il loop dei Cubes
			// nello shader (che li scorre in parallelo ai quad).
			m_World.CreateBox(spawn - glm::vec3(0.5f), spawn + glm::vec3(0.5f), material);

			m_Selection = Select(SelectionType::Box, (int)m_World.Boxes().size() - 1);
			m_GizmoOperation = ImGuizmo::TRANSLATE;
		}

		ImGui::SameLine();
		if (ImGui::Button("+ Quad")) {
			const ObjectId material = EnsureMaterialExists();

			Quad quad;
			quad.PositionLLC = glm::vec4(spawn - glm::vec3(0.5f, 0.5f, 0.0f), 0.0f);
			quad.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			quad.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			quad.Width = 1.0f;
			quad.Height = 1.0f;
			quad.Material = material;
			m_Selection = Select(SelectionType::Quad, m_World.AddQuad(quad));
			m_GizmoOperation = ImGuizmo::TRANSLATE;
		}

		// luci punto e spot (ADR 0009): lo spot nasce rivolto in basso
		for (int spot = 0; spot < 2; spot++) {
			if (ImGui::Button(spot ? "+ Spot light" : "+ Point light")) {
				PunctualLight light;
				light.Spot = spot == 1;
				light.Position = spawn;
				m_Selection = Select(SelectionType::Light, m_World.AddLight(light, spot ? "Spot light" : "Point light"));
				m_GizmoOperation = ImGuizmo::TRANSLATE;
			}
			if (spot == 0)
				ImGui::SameLine();
		}

		// Add (issue #27): le mesh e le luci di un file, davanti alla camera. Lo stesso file
		// aggiunto due volte si carica una volta sola: le istanze dividono la geometria.
		if (ImGui::Button("+ Model...")) {
			const auto selection = pfd::open_file("Add model", "models", FileFilters()).result();
			if (!selection.empty()) {
				const ObjectId material = EnsureMaterialExists();
				const int mesh = m_World.Add(selection[0], glm::translate(glm::mat4(1.0f), spawn), material);
				if (mesh >= 0) {
					m_Selection = Select(SelectionType::Mesh, mesh);
					m_GizmoOperation = ImGuizmo::TRANSLATE;
				}
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
		case SelectionType::Sphere: return static_cast<int>(m_World.Spheres().size());
		case SelectionType::Quad:   return static_cast<int>(m_World.Quads().size());
		case SelectionType::Box:    return static_cast<int>(m_World.Boxes().size());
		case SelectionType::Mesh:   return static_cast<int>(m_World.Meshes().size());
		case SelectionType::Light:  return static_cast<int>(m_World.PunctualLights().size());
		default:                    return 0;
		}
	}

	bool Application::CanDeleteSelection() const {
		return m_Selection.IsValid() && SelectedIndex() < SelectionCollectionSize(m_Selection.Type);
	}

	// Ogni rimozione fa scalare gli indici successivi della sua collezione: la selezione
	// corrente diventa insensata comunque vada, quindi si azzera PRIMA di toccare il World
	// (la finestra "Viewport" la userebbe piu' avanti in questo stesso frame).
	void Application::DeleteSelection() {
		if (!CanDeleteSelection()) return;

		const Selection sel = m_Selection;
		m_Selection = {};

		switch (sel.Type) {
		case SelectionType::Sphere: m_World.RemoveSphere(IndexOf(sel)); break;
		case SelectionType::Quad:   m_World.RemoveQuad(IndexOf(sel));   break;
		case SelectionType::Box:    m_World.RemoveBox(IndexOf(sel));    break;
		case SelectionType::Mesh:   m_World.RemoveMesh(IndexOf(sel));   break;
		case SelectionType::Light:  m_World.RemoveLight(IndexOf(sel));  break;
		default: break;
		}
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

			ImGui::Text("Delete %s?", GetSelectionName().c_str());

			switch (m_Selection.Type) {
			case SelectionType::Mesh:
				ImGui::TextDisabled("instance of %d triangles - the mesh asset stays loaded",
					m_World.AssetOf(m_World.Meshes()[SelectedIndex()]) ? static_cast<int>(m_World.AssetOf(m_World.Meshes()[SelectedIndex()])->Triangles.size()) : 0);
				break;
			case SelectionType::Box:
				ImGui::TextDisabled("also removes its 6 faces");
				break;
			default:
				break;
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Delete", ImVec2(120, 0))) {
				DeleteSelection();
				m_ShowDeletePopup = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))) {
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
	bool Application::DrawMaterialEditor(Material& material, int index) {
		// Editor in stile Principled (Blender): mostra concetti da artista e scrive nei campi
		// raw dello struct, che restano il contratto con lo shader. Le "probabilita'" di
		// importance sampling non compaiono: le derivo qui. Cosi' non possono piu' sommare
		// a un valore che sopprime il diffuso, che era il footgun della UI precedente.
		if (index >= 0 && index < (int)m_World.Materials().size())
			NameField(m_World.Materials()[index].Id, m_World.Materials()[index].Name);

		bool edited = false;
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
			ImGui::TextDisabled("metal: no diffuse or transmission, reflection tinted by albedo");

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
			ImGui::TextDisabled("dielectric F0. 0.5 = 0.04, the standard physical value");
			ImGui::TreePop();
		}

		// I campi derivati si "curano" ad ogni modifica: il riflesso dielettrico e' bianco
		// (non tinto) e la tinta del vetro segue la base color. Legacy con SpecularColor
		// colorato vengono normalizzati alla prima modifica.
		if (changed) {
			material.SpecularColor = glm::vec4(1.0f);
			material.RefractionColor = material.Color;
			edited = true;
		}

		ImGui::SeparatorText("Emission");
		if (ImGui::DragFloat("Emissive Strength", &material.EmissiveStrenght, 0.1f, 0.0f, FLT_MAX)) edited = true;
		if (ImGui::ColorEdit3("Emissive Color", glm::value_ptr(material.EmissiveColor))) edited = true;

		ImGui::SeparatorText("Base Color texture");
		int checkerMode = static_cast<int>(material.Checker + 0.5f);
		if (ImGui::Combo("Checker", &checkerMode, "Off\0UV (debug)\0World space\0\0")) {
			material.Checker = static_cast<float>(checkerMode);
			edited = true;
		}
		if (checkerMode != 0) {
			if (ImGui::DragFloat("Checker scale", &material.CheckerScale, 0.1f, 0.01f, 64.0f))
				edited = true;
		}

		// Nello shader checker e texture sono in catena else-if: il checker vince e la texture
		// non viene mai letta. Invece di mostrarli come se si combinassero, la disabilito e lo
		// dico, cosi' l'esclusione non e' piu' silenziosa.
		const bool checkerWins = checkerMode != 0;
		ImGui::BeginDisabled(checkerWins);
		if (TextureCombo("Albedo texture", material.AlbedoTexture, TextureKind::Color))
			edited = true;
		ImGui::EndDisabled();
		if (checkerWins)
			ImGui::TextDisabled("checker on: it takes precedence, the texture is ignored");

		// Normal map (ADR 0007): solo le mesh hanno le UV e i triangoli da cui si ricava la tangente
		ImGui::SeparatorText("Normal map");
		{
			if (TextureCombo("Normal map", material.NormalTexture, TextureKind::Data))
				edited = true;
			if (!material.NormalTexture.IsNil()) {
				if (ImGui::SliderFloat("Normal strength", &material.NormalStrength, 0.0f, 2.0f))
					edited = true;
				bool flipY = material.NormalFlipY > 0.5f;
				if (ImGui::Checkbox("Flip green (DirectX)", &flipY)) {
					material.NormalFlipY = flipY ? 1.0f : 0.0f;
					edited = true;
				}
				ImGui::TextDisabled("applies to meshes only");
			}
		}

		// Alpha mask (ADR 0008): i loader decidono quali materiali sono mascherati, perche' i loro
		// triangoli stanno in una geometria a parte del BLAS. Qui si cambia solo la soglia.
		if (material.AlphaCutoff > 0.0f) {
			ImGui::SeparatorText("Alpha mask");
			if (ImGui::SliderFloat("Alpha cutoff", &material.AlphaCutoff, 0.01f, 1.0f))
				edited = true;
		}

		ImGui::SeparatorText("Procedural noise");
		int noiseMode = static_cast<int>(material.NoiseType + 0.5f);
		if (ImGui::Combo("Noise", &noiseMode, "Off\0Perlin\0Turbulence\0Marble\0\0")) {
			material.NoiseType = static_cast<float>(noiseMode);
			edited = true;
		}
		if (noiseMode != 0) {
			if (ImGui::DragFloat("Noise scale", &material.NoiseScale, 0.05f, 0.01f, 64.0f)) edited = true;
			if (ImGui::SliderFloat("Noise blend", &material.NoiseBlend, 0.0f, 1.0f)) edited = true;

			// Turbulence e Marble sommano ottave: il costo per raggio e' lineare in questo
			// valore, e viene pagato a ogni rimbalzo su questo materiale.
			if (noiseMode >= 2) {
				int octaves = static_cast<int>(material.NoiseOctaves + 0.5f);
				if (ImGui::SliderInt("Octaves", &octaves, 1, 8)) {
					material.NoiseOctaves = static_cast<float>(octaves);
					edited = true;
				}
			}
			// La fase e' quello che trasforma il rumore in venature: senza, il marmo e' nebbia.
			if (noiseMode == 3) {
				if (ImGui::DragFloat("Turbulence phase", &material.NoiseTurbulence, 0.1f, 0.0f, 40.0f))
					edited = true;
			}

			if (ImGui::ColorEdit3("Noise color A", glm::value_ptr(material.NoiseColorA))) edited = true;
			if (ImGui::ColorEdit3("Noise color B", glm::value_ptr(material.NoiseColorB))) edited = true;

			ImGui::TextDisabled("Evaluated in world space: the object \"slides\" through the noise if you move it");
		}

		ImGui::Spacing();
		ImGui::TextDisabled("Material %d", index);
		return edited;
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
		if (m_MaterialEditorIndex < 0 || m_MaterialEditorIndex >= (int)m_World.Materials().size()) {
			m_ShowMaterialEditor = false;
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(340.0f, 0.0f), ImGuiCond_FirstUseEver);

		// il "###" fissa l'ID della finestra mentre il titolo cambia con l'indice
		const std::string title = "Material " + std::to_string(m_MaterialEditorIndex) + "###MaterialEditor";

		if (ImGui::Begin(title.c_str(), &m_ShowMaterialEditor)) {
			ImGui::PushItemWidth(170.0f);
			Material material = m_World.Materials()[m_MaterialEditorIndex];
			if (DrawMaterialEditor(material, m_MaterialEditorIndex))
				m_World.Set(m_World.Materials()[m_MaterialEditorIndex].Id, material);
			ImGui::PopItemWidth();
		}

		ImGui::End();
	}

	// Le texture sono asset (issue #26): i materiali le citano per id, e il layer lo sceglie il
	// RenderScene. Una texture che nessun materiale usa si puo' togliere, e libera il suo layer.
	void Application::DrawTextureManager() {
		ImGui::SeparatorText("Textures");

		if (m_World.Textures().empty())
			ImGui::TextDisabled("no textures loaded");

		ObjectId remove;
		for (const auto& texture : m_World.Textures()) {
			ImGui::PushID(texture.Name.c_str());
			const bool used = m_World.Assets().IsTextureUsed(texture.Id);
			ImGui::BeginDisabled(used);
			if (ImGui::SmallButton("x"))
				remove = texture.Id;
			ImGui::EndDisabled();
			if (used && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("used by a material");
			ImGui::SameLine();
			ImGui::Text("%s  (%s)", texture.Name.c_str(), texture.Kind == TextureKind::Color ? "color" : "data");
			ImGui::PopID();
		}
		if (!remove.IsNil())
			m_World.RemoveTexture(remove);

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
			ImGui::TextDisabled("(no files in res/textures)");
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

		// Una stessa immagine puo' servire come colore e come dato: sono due asset (sRGB e UNORM)
		const std::string& selected = m_TextureFiles[m_SelectedTextureFile];
		if (ImGui::Button("+ Color"))
			m_World.AddTexture(selected, TextureKind::Color); // gia' caricata = nessun doppione
		ImGui::SameLine();
		if (ImGui::Button("+ Data"))
			m_World.AddTexture(selected, TextureKind::Data);
		ImGui::TextDisabled("color = albedo (sRGB), data = normal map or alpha mask");
	}

	void Application::DrawMaterialsTab() {
		ImGui::Spacing();

		DrawTextureManager();
		ImGui::Spacing();

		if (ImGui::Button("+ Add material")) {
			OpenMaterialEditor(m_World.AddMaterial(CreateDefaultPrincipled()));
		}

		if (m_World.Materials().empty()) {
			ImGui::TextDisabled("no materials in this scene");
			return;
		}

		ImGui::Spacing();
		ImGui::PushItemWidth(200.0f);

		for (size_t i = 0; i < m_World.Materials().size(); i++) {
			Material material = m_World.Materials().at(i);

			ImGui::PushID((int)i);

			// pastiglia del colore accanto al nome: si riconosce il materiale a colpo d'occhio
			ImGui::ColorButton("##swatch", ImVec4(material.Color.r, material.Color.g, material.Color.b, 1.0f),
				ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(14.0f, 14.0f));
			ImGui::SameLine();

			if (ImGui::TreeNode(m_World.Materials()[i].Name.c_str())) {
				if (DrawMaterialEditor(material, (int)i))
					m_World.Set(m_World.Materials()[i].Id, material);
				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		ImGui::PopItemWidth();
	}

	// --------------------------------------------------------------- UI: tab Render

	// Le due camere (ADR 0011). La vista agganciata scrive nell'inquadratura quando si naviga;
	// in entrambe le modalita', se l'inquadratura cambia da fuori (un load, "Align", PT_CAM) la
	// vista la raggiunge. L'obiettivo e' sempre quello dell'inquadratura.
	void Application::SyncCameras(bool viewMoved) {
		if (m_Renderer.View.LockViewToCamera && viewMoved)
			m_World.MoveCamera(m_Camera.GetPosition(), m_Camera.GetDirection());
		const SceneCamera& shot = m_World.Camera();
		if (!shot.SamePose(m_LastShot))
			m_Camera.SetView(shot.Position, shot.Direction);
		if (m_Camera.m_VerticalFOV != shot.VerticalFOV) {
			m_Camera.m_VerticalFOV = shot.VerticalFOV;
			m_Camera.RecalculateProjection();
		}
		m_LastShot = shot;
	}

	// L'immagine del renderer come texture ImGui. Il renderer la ricrea al resize (la versione
	// cambia): si registra la nuova, e la vecchia si libera solo quando la GPU ha finito il frame
	// che la usava ancora.
	ImTextureID Application::DisplayTexture() {
		if (m_Renderer.GetDisplayView() == VK_NULL_HANDLE)
			return 0;
		if (m_DisplaySet == VK_NULL_HANDLE || m_DisplayVersion != m_Renderer.GetDisplayVersion()) {
			if (VkDescriptorSet old = m_DisplaySet; old != VK_NULL_HANDLE)
				m_Vulkan.Defer([old]() { ImGui_ImplVulkan_RemoveTexture(old); });
			m_DisplaySet = ImGui_ImplVulkan_AddTexture(m_Renderer.GetDisplaySampler(), m_Renderer.GetDisplayView(), VK_IMAGE_LAYOUT_GENERAL);
			m_DisplayVersion = m_Renderer.GetDisplayVersion();
		}
		return reinterpret_cast<ImTextureID>(m_DisplaySet);
	}

	void Application::DrawRenderTab() {
		ImGui::Spacing();
		ImGui::PushItemWidth(200.0f);

		// Preset (ADR 0010): punti di partenza. Le impostazioni si salvano con la scena.
		ImGui::SeparatorText("Preset");
		const std::pair<const char*, RenderSettings (*)()> presets[] = {
			{ "Draft", &RenderSettings::Draft }, { "Preview", &RenderSettings::Preview }, { "Final", &RenderSettings::Final } };
		for (size_t i = 0; i < std::size(presets); i++) {
			if (i > 0) ImGui::SameLine();
			if (ImGui::Button(presets[i].first))
				m_Renderer.Settings = presets[i].second();
		}

		ImGui::SeparatorText("Path tracing");
		ImGui::Checkbox("Enable Path Tracing", &m_Renderer.View.PathTracing);
		// le impostazioni dell'integratore fanno ripartire l'accumulo da sole (il Renderer le confronta)
		ImGui::SliderInt("Samples per pixel", &m_Renderer.Settings.SamplesPerPixel, 1, 16);
		ImGui::SliderInt("Ray depth", &m_Renderer.Settings.RayDepth, 1, 50);
		if (ImGui::Button("Reset Accumulation")) m_Renderer.RestartAccumulation();

		// Render scale: risoluzione ridotta SOLO mentre la camera si muove (piu' fluido),
		// piena risoluzione + accumulo da fermi. Nessun reset qui: agisce al prossimo movimento.
		{
			const char* labels[] = { "1.0 (full)", "0.75", "0.5", "0.25" };
			const float values[] = { 1.0f, 0.75f, 0.5f, 0.25f };
			int idx = 0;
			for (int i = 0; i < 4; i++) if (m_Renderer.Settings.RenderScale == values[i]) idx = i;
			if (ImGui::Combo("Render scale (while moving)", &idx, labels, 4))
				m_Renderer.Settings.RenderScale = values[idx];
		}

		ImGui::SeparatorText("Output");
		if (ImGui::Checkbox("Vsync", &m_Vsync)) {
			m_Vulkan.SetVsync(m_Vsync);
			m_Preferences.Vsync = m_Vsync;
			m_Preferences.Save();
		}

		// Esposizione e tonemap agiscono DOPO l'accumulo (fragment di post): cambiarli non
		// invalida i campioni gia' accumulati, e il Renderer non li conta fra le impostazioni che ripartono.
		ImGui::Checkbox("ACES tonemap", &m_Renderer.Settings.Tonemap);
		ImGui::DragFloat("Exposure", &m_Renderer.Settings.Exposure, 0.01f, 0.0f, 10.0f);

		// Il firefly clamp invece vive nel path tracer e cambia i campioni: va resettato.
		// Etichetta come luminanza massima; 0 = off. Sotto ~2 inizia a mangiare i riflessi.
		if (ImGui::DragFloat("Firefly clamp", &m_Renderer.Settings.FireflyClamp, 0.1f, 0.0f, 50.0f, "%.1f"))
			m_Renderer.Settings.FireflyClamp = std::max(m_Renderer.Settings.FireflyClamp, 0.0f);
		if (m_Renderer.Settings.FireflyClamp <= 0.0f)
			ImGui::TextDisabled("firefly clamp off");

		// La camera della scena e' l'inquadratura; la vista la segue o naviga libera (ADR 0011)
		ImGui::SeparatorText("Camera");
		// riagganciare la vista la riporta sull'inquadratura: il renderer lo vede e riparte
		if (ImGui::Checkbox("Lock view to camera", &m_Renderer.View.LockViewToCamera) && m_Renderer.View.LockViewToCamera)
			m_Camera.SetView(m_World.Camera().Position, m_World.Camera().Direction); // torna all'inquadratura
		ImGui::BeginDisabled(m_Renderer.View.LockViewToCamera);
		if (ImGui::Button("Align camera to view")) // la vista e' gia' li': l'immagine non cambia
			m_World.MoveCamera(m_Camera.GetPosition(), m_Camera.GetDirection());
		ImGui::EndDisabled();
		ImGui::TextDisabled(m_Renderer.View.LockViewToCamera ? "navigating moves the scene camera"
		                                                      : "free view: the scene camera stays put");

		// L'obiettivo cambia i raggi primari, quindi si riconsegna con SetCamera: l'accumulo
		// riparte (esposizione e tonemap invece agiscono dopo l'accumulo).
		SceneCamera camera = m_World.Camera();
		bool cameraChanged = ImGui::SliderFloat("Vertical FOV", &camera.VerticalFOV, 30.0f, 140.0f);
		if (ImGui::Button("Reset camera")) {
			const SceneCamera defaults;
			camera.Position = defaults.Position;
			camera.Direction = defaults.Direction;
			cameraChanged = true;
		}
		ImGui::TextDisabled("right mouse + WASD to navigate");

		ImGui::SeparatorText("Depth of field");
		if (ImGui::SliderFloat("Aperture", &camera.Aperture, 0.0f, 0.5f, "%.3f")) {
			camera.Aperture = std::max(camera.Aperture, 0.0f);
			cameraChanged = true;
		}

		const bool dofOff = camera.Aperture <= 0.0f;
		ImGui::BeginDisabled(dofOff);
		if (ImGui::DragFloat("Focus distance", &camera.FocusDistance, 0.05f, 0.01f, 1000.0f, "%.2f")) {
			camera.FocusDistance = std::max(camera.FocusDistance, 0.01f);
			cameraChanged = true;
		}
		if (cameraChanged)
			m_World.SetCamera(camera);

		// pick del punto di fuoco: arma la modalita', il prossimo click nel viewport
		// mette a fuoco esattamente li'
		if (m_FocusPickArmed) {
			if (ImGui::Button("Cancel pick")) m_FocusPickArmed = false;
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "click a point in the scene...");
		}
		else if (ImGui::Button("Pick focus (click an object)")) {
			m_FocusPickArmed = true;
		}
		ImGui::EndDisabled();

		if (dofOff)
			ImGui::TextDisabled("aperture 0: DOF off (pinhole)");

		ImGui::PopItemWidth();

		ImGui::SeparatorText("Vulkan");
		ImGui::TextDisabled("Device: %s", m_Vulkan.DeviceName());
	}

	// ---------------------------------------------------------------- UI: tab Debug

	void Application::DrawDebugTab() {
		ImGui::Spacing();

		ImGui::SeparatorText("AOV / Denoiser");
		// Vista dei feature buffer del primo hit (fondamenta del denoiser). Beauty = immagine
		// normale; gli altri mostrano l'AOV crudo (senza esposizione/ACES).
		// reset dell'accumulo al cambio vista: le viste AOV scrivono in imgOutput (condiviso
		// con l'accumulatore del colore), quindi tornando a Beauty il colore va ripartito da capo.
		ImGui::Combo("View", &m_Renderer.View.AOVView, "Beauty\0Albedo\0Normal\0Depth\0History\0\0");
		if (m_Renderer.View.AOVView == 4)
			ImGui::TextDisabled("green = mature history, red = restarted (disocclusion)");
		else if (m_Renderer.View.AOVView != 0)
			ImGui::TextDisabled("AOV debug view: no tonemap");

		// Riproiezione temporale (ADR 0002)
		ImGui::Spacing();
		ImGui::Checkbox("Temporal reprojection", &m_Renderer.Settings.Reproject);
		if (m_Renderer.Settings.Reproject) {
			ImGui::SliderInt("History cap", &m_Renderer.Settings.HistoryCap, 1, 128);
			ImGui::TextDisabled("cap solo in movimento; da fermo N cresce libero");
		} else {
			ImGui::TextDisabled("off: moving the camera resets accumulation");
		}

		// Denoiser à-trous: e' un view filter read-only, non tocca l'accumulo -> niente reset.
		// Next Event Estimation: cambia l'integratore -> reset dell'accumulo per non
		// mischiare campioni con e senza NEE.
		ImGui::Spacing();
		ImGui::Checkbox("Next Event Estimation", &m_Renderer.Settings.NEE);
		ImGui::TextDisabled(m_Renderer.Settings.NEE
			? "samples lights directly (needs an emitter: emissive geometry or the sun)"
			: "off: lights are found only by random bounces (more noise)");

		ImGui::Spacing();
		ImGui::Checkbox("Denoise (a-trous)", &m_Renderer.Settings.Denoise);
		if (m_Renderer.Settings.Denoise) {
			if (m_Renderer.View.AOVView != 0) {
				ImGui::TextDisabled("active in Beauty view only");
			} else {
				ImGui::SliderFloat("Strength", &m_Renderer.Settings.DenoiseStrength, 0.0f, 1.0f, "%.2f");
				ImGui::Checkbox("Fade with accumulation", &m_Renderer.Settings.DenoiseAutoFade);
				if (m_Renderer.Settings.DenoiseAutoFade)
					ImGui::TextDisabled("fades out as the image converges (~1/sqrt N)");
				ImGui::SliderFloat("Sigma luminance", &m_Renderer.Settings.DenoiseCPhi, 0.05f, 10.0f, "%.2f");
				ImGui::SliderFloat("Sigma normal", &m_Renderer.Settings.DenoiseNPhi, 1.0f, 128.0f, "%.0f");
				ImGui::SliderFloat("Sigma depth", &m_Renderer.Settings.DenoisePPhi, 0.05f, 10.0f, "%.2f");
				ImGui::SliderFloat("Sigma albedo", &m_Renderer.Settings.DenoiseAPhi, 0.02f, 1.0f, "%.2f");
				ImGui::TextDisabled("view filter: does not touch accumulation");
			}
		}

		ImGui::SeparatorText("Picking");
		ImGui::Text("hovered window : %s", HoveredWindowName());
		ImGui::Text("viewport hover : %s", m_ViewportHovered ? "YES" : "NO");
		ImGui::Text("viewport rect  : (%.0f,%.0f) %.0f x %.0f",
			m_ViewportPos.x, m_ViewportPos.y, m_ViewportSize.x, m_ViewportSize.y);
		ImGui::Text("pick requests  : %d", m_PickRequestCount);
		ImGui::Text("last pixel     : (%d, %d)", m_LastPickPixel.x, m_LastPickPixel.y);
		ImGui::Text("last result    : type=%d index=%d", m_LastPickType, m_LastPickIndex);
		ImGui::TextDisabled("-2 = not written by the shader, -1 = ray missed");

		// Bypassa il gate del mouse: se questo seleziona ma il click no, il problema
		// e' nel gate; se non seleziona nemmeno questo, e' nella pipeline di picking.
		if (ImGui::Button("Pick at viewport center")) {
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
			return glm::vec3(m_World.Spheres().at(SelectedIndex()).Position);
		case SelectionType::Quad: {
			const Quad& q = m_World.Quads().at(SelectedIndex());
			return glm::vec3(q.PositionLLC)
				 + glm::vec3(q.U) * (q.Width * 0.5f)
				 + glm::vec3(q.V) * (q.Height * 0.5f);
		}
		case SelectionType::Box: {
			const Box& b = m_World.Boxes().at(SelectedIndex());
			return 0.5f * (glm::vec3(b.Min) + glm::vec3(b.Max));
		}
		case SelectionType::Mesh: {
			const AABB bounds = m_World.WorldBounds(m_World.Meshes().at(SelectedIndex()));
			return 0.5f * (bounds.min + bounds.max);
		}
		case SelectionType::Light:
			return m_World.PunctualLights().at(SelectedIndex()).Position;
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
			const Sphere& s = m_World.Spheres().at(SelectedIndex());
			glm::vec3 center(s.Position);
			outMin = center - glm::vec3(s.Position.w);
			outMax = center + glm::vec3(s.Position.w);
			break;
		}
		case SelectionType::Quad: {
			const Quad& q = m_World.Quads().at(SelectedIndex());
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
			const Box& b = m_World.Boxes().at(SelectedIndex());
			outMin = glm::vec3(b.Min);
			outMax = glm::vec3(b.Max);
			break;
		}
		case SelectionType::Mesh: {
			const AABB bounds = m_World.WorldBounds(m_World.Meshes().at(SelectedIndex()));
			outMin = bounds.min;
			outMax = bounds.max;
			break;
		}
		case SelectionType::Light: {
			// una luce non ha estensione: un piccolo cubo attorno alla posizione per vederla
			const glm::vec3 p = m_World.PunctualLights().at(SelectedIndex()).Position;
			outMin = p - glm::vec3(0.1f);
			outMax = p + glm::vec3(0.1f);
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
	std::string Application::ObjectName(const Selection& selection) const {
		const int i = IndexOf(selection);
		if (i < 0) return {};
		switch (selection.Type) {
		case SelectionType::Sphere: return m_World.Spheres()[i].Name;
		case SelectionType::Quad:   return m_World.Quads()[i].Name;
		case SelectionType::Box:    return m_World.Boxes()[i].Name;
		case SelectionType::Mesh:   return m_World.Meshes()[i].Name;
		case SelectionType::Light:  return m_World.PunctualLights()[i].Name;
		default:                    return {};
		}
	}

	std::string Application::GetSelectionName() const {
		std::string name;
		switch (m_Selection.Type) {
		case SelectionType::Sphere: name = "Sphere"; break;
		case SelectionType::Quad:   name = "Quad";   break;
		case SelectionType::Box:    name = "Box";    break;
		case SelectionType::Light:
			name = m_World.PunctualLights().at(SelectedIndex()).Spot ? "Spot light" : "Point light";
			break;
		case SelectionType::Mesh:   name = "Mesh";   break;
		default: return "?";
		}
		return name + " \"" + ObjectName(m_Selection) + "\"";
	}

	std::string Application::GetSelectionLabel() const {
		if (!m_Selection.IsValid())
			return "No selection  -  click an object to select it";

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

		// uno spot non si vede: una linea mostra dove punta
		if (m_Selection.Type == SelectionType::Light) {
			const PunctualLight& light = m_World.PunctualLights().at(SelectedIndex());
			ImVec2 tip;
			if (light.Spot && WorldToScreen(viewProjection, light.Position + light.Direction * 0.75f, m_ViewportPos, m_ViewportSize, tip)
			    && WorldToScreen(viewProjection, light.Position, m_ViewportPos, m_ViewportSize, pivot))
				drawList->AddLine(pivot, tip, accent, 2.0f);
		}
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
			Sphere s = m_World.Spheres().at(SelectedIndex());
			glm::vec3 center = glm::vec3(delta * glm::vec4(glm::vec3(s.Position), 1.0f));

			// una sfera ha un solo raggio: la scala puo' essere solo uniforme
			float uniformScale = (axisScale.x + axisScale.y + axisScale.z) / 3.0f;
			s.Position = glm::vec4(center, glm::max(s.Position.w * uniformScale, 1e-4f));
			m_World.Set(m_Selection.Id, s);
			break;
		}
		case SelectionType::Quad: {
			Quad q = m_World.Quads().at(SelectedIndex());

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
			m_World.Set(m_Selection.Id, q);
			break;
		}
		case SelectionType::Box: {
			Box b = m_World.Boxes().at(SelectedIndex());
			glm::vec3 center = 0.5f * (glm::vec3(b.Min) + glm::vec3(b.Max));
			glm::vec3 size = glm::vec3(b.Max) - glm::vec3(b.Min);

			center = glm::vec3(delta * glm::vec4(center, 1.0f));
			// il box e' axis-aligned: si puo' scalare, non ruotare
			// UpdateBox normalizza gli assi, quindi una dimensione nulla darebbe NaN
			size = glm::max(size * axisScale, glm::vec3(1e-3f));

			b.Min = glm::vec4(center - size * 0.5f, 0.0f);
			b.Max = glm::vec4(center + size * 0.5f, 0.0f);
			m_World.Set(m_Selection.Id, b); // riscrive i 6 quad che compongono il box
			break;
		}
		case SelectionType::Mesh: {
			// nessuna ricostruzione dei BLAS: cambia solo la trasformazione dell'istanza
			Mesh mesh = m_World.Meshes().at(SelectedIndex());
			mesh.Transform = delta * mesh.Transform;
			m_World.Set(m_Selection.Id, mesh);
			break;
		}
		case SelectionType::Light: {
			// la scala non ha senso su una luce delta: contano posizione e direzione
			PunctualLight light = m_World.PunctualLights().at(SelectedIndex());
			light.Position = glm::vec3(delta * glm::vec4(light.Position, 1.0f));
			const glm::vec3 dir = linear * light.Direction;
			if (glm::length(dir) > 1e-6f)
				light.Direction = glm::normalize(dir);
			m_World.Set(m_Selection.Id, light);
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

		if (SelectedIndex() < 0)
			m_Selection = {};
	}

	ObjectId Application::IdOf(SelectionType type, int index) const {
		auto idAt = [index](const auto& list) { return index >= 0 && index < (int)list.size() ? list[index].Id : ObjectId{}; };
		switch (type) {
		case SelectionType::Sphere: return idAt(m_World.Spheres());
		case SelectionType::Quad:   return idAt(m_World.Quads());
		case SelectionType::Box:    return idAt(m_World.Boxes());
		case SelectionType::Mesh:   return idAt(m_World.Meshes());
		case SelectionType::Light:  return idAt(m_World.PunctualLights());
		default:                    return {};
		}
	}

	// ponytail: ricerca lineare per id a ogni uso; con migliaia di oggetti servira' una mappa
	int Application::IndexOf(const Selection& selection) const {
		if (!selection.IsValid())
			return -1;
		switch (selection.Type) {
		case SelectionType::Sphere: return m_World.Spheres().IndexOf(selection.Id);
		case SelectionType::Quad:   return m_World.Quads().IndexOf(selection.Id);
		case SelectionType::Box:    return m_World.Boxes().IndexOf(selection.Id);
		case SelectionType::Mesh:   return m_World.Meshes().IndexOf(selection.Id);
		case SelectionType::Light:  return m_World.PunctualLights().IndexOf(selection.Id);
		default:                    return -1;
		}
	}

	// Il buffer di ogni oggetto vive tra un frame e l'altro: mentre si scrive ImGui lo modifica,
	// quando il campo e' fermo si riallinea al nome (che puo' cambiare da fuori, es. un load).
	void Application::NameField(const ObjectId& id, const std::string& name) {
		std::array<char, 128>& buffer = m_NameBuffers[id];
		if (buffer[0] == '\0' && !name.empty())
			snprintf(buffer.data(), buffer.size(), "%s", name.c_str());
		ImGui::InputText("Name", buffer.data(), buffer.size());
		if (ImGui::IsItemDeactivatedAfterEdit())
			m_World.Rename(id, buffer.data()); // unico nella lista: puo' diventare "nome.001"
		if (!ImGui::IsItemActive())
			snprintf(buffer.data(), buffer.size(), "%s", name.c_str());
	}

	void Application::SetSelectionFromPick(int objectType, int objectIndex) {
		if (objectType < 0 || objectIndex < 0) {
			m_Selection = {};
			return;
		}

		// lo shader non conosce i box: riporta l'indice del quad colpito, e i 6 quad
		// consecutivi a partire da Box::index appartengono a quel box
		if (objectType == static_cast<int>(SelectionType::Quad)) {
			for (size_t b = 0; b < m_World.Boxes().size(); b++) {
				int firstQuad = static_cast<int>(m_World.Boxes()[b].index);
				if (objectIndex >= firstQuad && objectIndex < firstQuad + 6) {
					m_Selection = Select(SelectionType::Box, static_cast<int>(b));
					return;
				}
			}
		}

		m_Selection = Select(static_cast<SelectionType>(objectType), objectIndex);
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
					SceneCamera camera = m_World.Camera();
					camera.FocusDistance = pickedDistance;
					m_World.SetCamera(camera); // il fuoco cambia i raggi primari
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
			ApplyDelta(delta); // passa dal World: l'accumulo riparte da solo
		}
	}

	void Application::Render(float deltaTime) {
		// Test/CI headless: PT_VIEWPORT=<L>x<A> fissa la risoluzione di render. Il layout ImGui
		// cambia da un avvio all'altro e renderebbe non confrontabili immagini e tempi.
		if (const char* vp = getenv("PT_VIEWPORT"))
			sscanf(vp, "%ux%u", &m_ViewportWidth, &m_ViewportHeight);

		m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Renderer.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Renderer.Render(m_Camera.GetRenderView(), m_World);
	}
}
