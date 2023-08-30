#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "Application.h"
#include <glm/gtc/type_ptr.hpp>

static Application* s_Instance = nullptr;

Application::Application() : m_Window(nullptr), m_Height(900), m_Width(1600), m_IsRunning(true),
                             m_Camera(45.0f, 0.1f, 100.0f) {
    s_Instance = this;

	InitializeMaterials();
	InitializeScene();
}

Application::~Application() {
    //Shutdown();
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
		glfwPollEvents();

		if (m_Camera.OnUpdate(m_DeltaTime))
			m_Renderer.ResetPathTracingCounter();

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

		RenderUI(m_DeltaTime);
		Render(m_DeltaTime);

		// Rendering
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

void Application::InitializeMaterials() {
	Material redMaterial, blueMaterial, pinkMaterial, lightMaterial, greenMaterial, whiteMaterial, glassMaterial;
	redMaterial.Name = "Red Material";
	redMaterial.Color = { 1.0f, 0.0f, 0.0f };
	redMaterial.Roughness = 0.5f;
	blueMaterial.Name = "Blue Material";
	blueMaterial.Color = { 0.0f, 0.0f, 1.0f };
	blueMaterial.Roughness = 0.0f;
	pinkMaterial.Name = "Pink Material";
	pinkMaterial.Color = { 0.2f, 0.3f, 1.0f };
	pinkMaterial.Roughness = 1.0f;
	lightMaterial.Name = "Light";
	lightMaterial.Color = { 0.88f, 0.83f, 0.3f };
	lightMaterial.Roughness = 1.0f;
	lightMaterial.EmissiveColor = lightMaterial.Color;
	lightMaterial.EmissiveStrenght = 1.0f;
	greenMaterial.Name = "Green Material";
	greenMaterial.Color = { 0.0f, 1.0f, 0.0f };
	greenMaterial.Roughness = 1.0f;
	whiteMaterial.Name = "White Material";
	whiteMaterial.Color = { 1.0f, 1.0f, 1.0f };
	whiteMaterial.Roughness = 1.0f;
	glassMaterial.Name = "Glass";
	glassMaterial.Color = { 1.0f, 1.0f, 1.0f };
	glassMaterial.Refractive = true;
	glassMaterial.RefractionRatio = 1.7f;
	m_World.Materials.push_back(redMaterial);
	m_World.Materials.push_back(blueMaterial);
	m_World.Materials.push_back(pinkMaterial);
	m_World.Materials.push_back(lightMaterial);
	m_World.Materials.push_back(whiteMaterial);
	m_World.Materials.push_back(greenMaterial);
	m_World.Materials.push_back(glassMaterial);

	m_World.BackgroundColor = glm::vec3(0.6f, 0.7f, 0.9f);
}

void setupSpheres(World& m_World) {	
	{
		Sphere sphere;
		sphere.Position = { -0.8f, 0.0f, 0.0f };
		sphere.Radius = 1.0f;
		sphere.MaterialIndex = 0;
		m_World.Spheres.push_back(sphere);
	}
	{
		Sphere sphere;
		sphere.Position = { 1.0f, -0.2f, 0.0f };
		sphere.Radius = 0.8f;
		sphere.MaterialIndex = 1;
		m_World.Spheres.push_back(sphere);
	}
	{
		Sphere sphere;
		sphere.Position = { 0.0f, -101.0f, 0.0f };
		sphere.Radius = 100.0f;
		sphere.MaterialIndex = 2;
		m_World.Spheres.push_back(sphere);
	}
	{
		Sphere sphere;
		sphere.Position = { 1.0f, 1.0f, 1.0f };
		sphere.Radius = 0.1f;
		sphere.MaterialIndex = 3;
		m_World.Spheres.push_back(sphere);
	}
}

void setupCornellBox(World& m_World) {
	{
		Sphere sphere;
		sphere.Position = { 0.0f, 0.0f, 0.0f };
		sphere.Radius = 1.0f;
		sphere.MaterialIndex = 0;
		m_World.Spheres.push_back(sphere);
	}
	// left green
	{
		Quad quad;
		quad.PositionLLC = { -2.0f, -2.0f, 4.0f };
		quad.U = { 0.0f, 0.0f, -1.0f };
		quad.V = { 0.0f, 1.0f, 0.0f };
		quad.Width = 4.0f;
		quad.Height = 4.0f;
		quad.MaterialIndex = 5;

		m_World.Quads.push_back(quad);
	}

	// back white
	{
		Quad quad;
		quad.PositionLLC = { -2.0f, -2.0f, 0.0f };
		quad.U = { 1.0f, 0.0f, 0.0f };
		quad.V = { 0.0f, 1.0f, 0.0f };
		quad.Width = 4.0f;
		quad.Height = 4.0f;
		quad.MaterialIndex = 4;

		m_World.Quads.push_back(quad);
	}

	// right red
	{
		Quad quad;
		quad.PositionLLC = { 2.0f, -2.0f, 0.0f };
		quad.U = { 0.0f, 0.0f, 1.0f };
		quad.V = { 0.0f, 1.0f, 0.0f };
		quad.Width = 4.0f;
		quad.Height = 4.0f;
		quad.MaterialIndex = 0;

		m_World.Quads.push_back(quad);
	}
	

	// top white
	{
		Quad quad;
		quad.PositionLLC = { -2.0f, 2.0f, 0.0f };
		quad.U = { 1.0f, 0.0f, 0.0f };
		quad.V = { 0.0f, 0.0f, 1.0f };
		quad.Width = 4.0f;
		quad.Height = 4.0f;
		quad.MaterialIndex = 4;

		m_World.Quads.push_back(quad);
	}

	// bot white
	{
		Quad quad;
		quad.PositionLLC = { -2.0f, -2.0f, 4.0f };
		quad.U = { 1.0f, 0.0f, 0.0f };
		quad.V = { 0.0f, 0.0f, -1.0f };
		quad.Width = 4.0f;
		quad.Height = 4.0f;
		quad.MaterialIndex = 4;

		m_World.Quads.push_back(quad);
	}

	// top light
	{
		Quad quad;
		quad.PositionLLC = { -0.25f, 1.95f, 2.00f };
		quad.U = { 1.0f, 0.0f, 0.0f };
		quad.V = { 0.0f, 0.0f, 1.0f };
		quad.Width = 0.5f;
		quad.Height = 0.5f;
		quad.MaterialIndex = 3;

		m_World.Quads.push_back(quad);
	}
}

void setupAltScene(World& world) {
	{
		Sphere sphere;
		sphere.Position = { 0.0f, -1.0f, 2.0f };
		sphere.Radius = 1.0f;
		sphere.MaterialIndex = 3;
		world.Spheres.push_back(sphere);
	}

	// bot white
	{
		Quad quad;
		quad.PositionLLC = { -2.0f, -2.0f, 4.0f };
		quad.U = { 1.0f, 0.0f, 0.0f };
		quad.V = { 0.0f, 0.0f, -1.0f };
		quad.Width = 4.0f;
		quad.Height = 4.0f;
		quad.MaterialIndex = 0;

		world.Quads.push_back(quad);
	}
}

void Application::InitializeScene() {
	setupSpheres(m_World);
	//setupCornellBox(m_World);
	//setupAltScene(m_World);
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
	ImGui::Begin("Settings");
	ImGui::Text("FPS: %f", m_FPS);
	ImGui::Text("Last Render: %.3fms", deltaTime * 1000);
	ImGui::Separator();

	ImGui::Spacing();
	ImGui::SeparatorText("Engine Configuration");
	ImGui::Checkbox("Enable Path Tracing", &m_Renderer.PathTracing);
	if (ImGui::Button("Reset Accumulation"))
		m_Renderer.ResetPathTracingCounter();

	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Separator();

	ImGui::SeparatorText("Scene Configurations");

	ImGui::Spacing();
	ImGui::Text("Background");
	ImGui::ColorEdit3("Background Color", glm::value_ptr(m_World.BackgroundColor));
	ImGui::DragFloat("Ambient Occlusion Intensity",&m_World.AmbientOcclusionIntensity, 0.1f, 0.0f, 1.0f);

	ImGui::Spacing();
	ImGui::Spacing();

	ImGui::Text("Objects:");

	if (ImGui::TreeNode("Spheres") && m_World.Spheres.size() > 0) {
		for (size_t i = 0; i < m_World.Spheres.size(); i++) {
			Sphere& sphere = m_World.Spheres.at(i);

			ImGui::PushID(i);
			ImGui::DragFloat3("Position", glm::value_ptr(sphere.Position), 0.1f);
			ImGui::DragFloat("Radius", &sphere.Radius, 0.1f, 0.0f, 10.0f);
			if (ImGui::BeginCombo("Material", m_World.Materials.at(sphere.MaterialIndex).Name, 0 << 1)) {
				for (int i = 0; i < m_World.Materials.size(); i++) {
					const bool isSelected = sphere.MaterialIndex == i;

					if (ImGui::Selectable(m_World.Materials.at(i).Name, isSelected)) {
						m_Renderer.ResetPathTracingCounter();
						sphere.MaterialIndex = i;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}
			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Quads") && m_World.Quads.size() > 0) {
		for (size_t i = 0; i < m_World.Quads.size(); i++) {
			Quad& quad = m_World.Quads.at(i);

			ImGui::PushID(i);
			ImGui::DragFloat3("Position", glm::value_ptr(quad.PositionLLC), 0.1f);
			ImGui::DragFloat3("U", glm::value_ptr(quad.U), 0.1f);
			ImGui::DragFloat3("V", glm::value_ptr(quad.V), 0.1f);
			ImGui::DragFloat("Width", &quad.Width, 0.1f);
			ImGui::DragFloat("Height", &quad.Height, 0.1f);
			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Materials")) {
		for (size_t i = 0; i < m_World.Materials.size(); i++) {
			Material& material = m_World.Materials.at(i);

			ImGui::PushID(i);
			ImGui::SeparatorText(material.Name);
			ImGui::ColorEdit3("Color", glm::value_ptr(material.Color));
			ImGui::DragFloat("Roughness", &material.Roughness, 0.05f, 0.0f, 1.0f);
			ImGui::DragFloat("Emissive Strenght", &material.EmissiveStrenght, 0.1f, 0.0f, FLT_MAX);
			ImGui::ColorEdit3("Emissive Color", glm::value_ptr(material.EmissiveColor));
			ImGui::Checkbox("Refractive", &material.Refractive);
			ImGui::DragFloat("Refraction Index", &material.RefractionRatio, 0.1f, 1.0f, 3.0f);
			ImGui::Separator();
			ImGui::PopID();
		}
	}

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
    m_Renderer.Render(m_Camera, m_World);
}

