#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "Application.h"

static Application* s_Instance = nullptr;

Application::Application() : m_Window(nullptr), m_LastFrame(0), m_Height(600), m_Width(600), m_IsRunning(true),
                             m_Camera(45.0f, 0.1f, 100.0f) {
    s_Instance = this;
}

Application::~Application() {
    Shutdown();
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
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

	// Create window with graphics context
	m_Window = glfwCreateWindow(width, height, "PathTracer", nullptr, nullptr);

	if (m_Window == nullptr)
		return false;

	int bufferW, bufferH;
	glfwGetFramebufferSize(m_Window, &bufferW, &bufferH);

	glfwMakeContextCurrent(m_Window);
	glfwSwapInterval(1); // Enable vsync

	if (glewInit() != GLEW_OK) {
		Shutdown();
		return false;
	}

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
	//io.ConfigViewportsNoAutoMerge = true;
	//io.ConfigViewportsNoTaskBarIcon = true;

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	return true;
}

void Application::RunLoop() {
	ImGuiIO& io = ImGui::GetIO();

	while (!glfwWindowShouldClose(m_Window) && m_IsRunning) {
		float currentFrame = glfwGetTime();
		float deltaTime = currentFrame - m_LastFrame;
		m_LastFrame = currentFrame;

		glfwPollEvents();

		ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

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

		RenderUI(deltaTime);
		Render(deltaTime);

		// Rendering
		ImGui::Render();

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}

		glfwSwapBuffers(m_Window);
	}
}

void Application::Shutdown() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(m_Window);
	glfwTerminate();
}

void Application::RenderUI(float deltaTime) {
	ImGui::Begin("Settings");
	ImGui::Text("Last Render: %.3fms", deltaTime);

	ImGui::End();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Viewport");
	m_ViewportWidth = (uint32_t)ImGui::GetContentRegionAvail().x;
	m_ViewportHeight = (uint32_t)ImGui::GetContentRegionAvail().y;

    auto image = m_Renderer.GetRenderedImage();
    if(image)
        ImGui::Image(image->GetTexture(), {(float)image->GetWidth(), (float)image->GetHeight()}, ImVec2(0, 1), ImVec2(1, 0));

	ImGui::End();
    ImGui::PopStyleVar();
}

void Application::Render(float deltaTime) {
    m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);
    m_Renderer.OnResize(m_ViewportWidth, m_ViewportHeight);
    m_Renderer.Render(m_Camera);
}
