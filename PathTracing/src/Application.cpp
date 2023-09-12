#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "Application.h"
#include <glm/gtc/type_ptr.hpp>
#include "Random.h"

static Application* s_Instance = nullptr;

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
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
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

	m_Camera.OnResize(m_Width, m_Height);
	m_Renderer.Initialize(m_Camera, m_World);

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

		// Rendering
		RenderUI(m_DeltaTime);
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

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
	ImGui::SeparatorText("ENGINE CONFIGURATIONS");
	ImGui::PopStyleColor();
	ImGui::Spacing();
	if (ImGui::Checkbox("Vsync", &m_Vsync))
		glfwSwapInterval(m_Vsync);
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
	ImGui::Checkbox("Enable Path Tracing", &m_Renderer.PathTracing);
	if (ImGui::Button("Reset Accumulation"))
		m_Renderer.ResetPathTracingCounter();

	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Separator();

	ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255));
	ImGui::SeparatorText("SCENE CONFIGURATIONS");
	ImGui::PopStyleColor();	

	ImGui::Separator();
	ImGui::Spacing();
	ImGui::Text("Background");
	if(ImGui::ColorEdit3("Background Color", glm::value_ptr(m_World.BackgroundColor)))
		m_Renderer.ResetPathTracingCounter();
	if(ImGui::DragFloat("Ambient Occlusion Intensity",&m_World.AmbientOcclusionIntensity, 0.1f, 0.0f, 1.0f))
		m_Renderer.ResetPathTracingCounter();

	ImGui::Spacing();
	ImGui::Spacing();
	ImGui::Text("Select the scene");
	if (ImGui::Combo("Current Scene", &m_World.CurrentScene, "TWO SPHERES\0RANDOM SPHERES\0CORNELL BOX\0\0")) {
		m_World.DestroyScene();
		m_Renderer.ResetPathTracingCounter();
		m_World.LoadScene();
	}

	ImGui::Spacing();
	ImGui::Spacing();

	ImGui::Text("Objects:");

	// if (ImGui::TreeNode("Spheres") && m_World.Spheres.size() > 0) {
	// 	for (size_t i = 0; i < m_World.Spheres.size(); i++) {
	// 		Sphere& sphere = m_World.Spheres.at(i);

	// 		ImGui::PushID(i);
	// 		ImGui::DragFloat3("Position", glm::value_ptr(sphere.Position), 0.1f);
	// 		ImGui::DragFloat("Radius", &sphere.Radius, 0.1f, 0.0f, 10.0f);
	// 		ImGui::DragInt("Material", &sphere.MaterialIndex, 1.0f, 0, (int)m_World.Materials.size() - 1);
	// 		/*
	// 		if (ImGui::BeginCombo("Material", m_World.Materials.at(sphere.MaterialIndex).Name, 0 << 1)) {
	// 			for (int i = 0; i < m_World.Materials.size(); i++) {
	// 				const bool isSelected = sphere.MaterialIndex == i;

	// 				if (ImGui::Selectable(m_World.Materials.at(i).Name, isSelected)) {
	// 					m_Renderer.ResetPathTracingCounter();
	// 					sphere.MaterialIndex = i;
	// 				}

	// 				if (isSelected)
	// 					ImGui::SetItemDefaultFocus();
	// 			}

	// 			ImGui::EndCombo();
	// 		}
	// 		*/
	// 		ImGui::Separator();
	// 		ImGui::PopID();
	// 	}

	// 	ImGui::TreePop();
	// }

	if (ImGui::TreeNode("Spheres") && m_World.Spheres.size() > 0) {
		for (size_t i = 0; i < m_World.Spheres.size(); i++) {
			Sphere& sphere = m_World.Spheres.at(i);

			ImGui::PushID(i);
			if (ImGui::DragFloat3("Position", glm::value_ptr(sphere.Position), 0.1f)) m_Renderer.ResetPathTracingCounter();
			if (ImGui::DragFloat("Radius", &sphere.Position.w, 0.1f, 0.0f, 10.0f)) m_Renderer.ResetPathTracingCounter();
			if (ImGui::DragFloat("Material", &sphere.MaterialIndex, 1.0f, 0, (int)m_World.Materials.size() - 1)) m_Renderer.ResetPathTracingCounter();
			/*
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
			*/
			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Quads") && m_World.Quads.size() > 0) {
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

	if (ImGui::TreeNode("Boxez") && m_World.Boxes.size() > 0) {
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

	ImGui::Spacing();
	ImGui::Spacing();

	if (ImGui::CollapsingHeader("Materials")) {
		for (size_t i = 0; i < m_World.Materials.size(); i++) {
			Material& material = m_World.Materials.at(i);

			ImGui::PushID(i);
			//ImGui::SeparatorText(material.Name);
			if (ImGui::ColorEdit3("Color", glm::value_ptr(material.Color))) m_Renderer.ResetPathTracingCounter();
			if (ImGui::DragFloat("Roughness", &material.Roughness, 0.05f, 0.0f, 1.0f)) m_Renderer.ResetPathTracingCounter();
			if (ImGui::DragFloat("Emissive Strenght", &material.EmissiveStrenght, 0.1f, 0.0f, FLT_MAX)) m_Renderer.ResetPathTracingCounter();
			if (ImGui::ColorEdit3("Emissive Color", glm::value_ptr(material.EmissiveColor))) m_Renderer.ResetPathTracingCounter();
			//ImGui::Checkbox("Refractive", &material.Refractive);
			if (ImGui::DragFloat("Refraction Index", &material.RefractionRatio, 0.1f, 1.0f, 3.0f)) m_Renderer.ResetPathTracingCounter();
			ImGui::Separator();
			ImGui::PopID();
		}
	}

	ImGui::End();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Viewport");
	m_ViewportWidth = (uint32_t)ImGui::GetContentRegionAvail().x;
	m_ViewportHeight = (uint32_t)ImGui::GetContentRegionAvail().y;

	ImVec2 pos = ImGui::GetCursorScreenPos();
    auto image = m_Renderer.GetRenderedImage();
	if(image)
		ImGui::GetWindowDrawList()->AddImage(
			image->GetTexture(),
			ImVec2(pos.x, pos.y),
			ImVec2(pos.x + m_ViewportWidth, pos.y + m_ViewportHeight),
			ImVec2(0, 1),
			ImVec2(1, 0)
		);

	ImGui::End();
    ImGui::PopStyleVar();
}

void Application::Render(float deltaTime) {
    m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);
    m_Renderer.OnResize(m_ViewportWidth, m_ViewportHeight);
    m_Renderer.Render(m_Camera, m_World);
}

