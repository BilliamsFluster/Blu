#include "Blupch.h"
#include "ImGuiLayer.h"
#include "imgui.h"
#include "Blu/Platform/OpenGL/ImGuiOpenGLRenderer.h"
#include <GLFW/glfw3.h>
#include "Blu/Core/Application.h"


namespace Blu
{
	namespace Layers
	{
		ImGuiLayer::ImGuiLayer()
			:Layer("ImGuiLayer")
		{
		}
		ImGuiLayer::~ImGuiLayer()
		{
		}
		void ImGuiLayer::OnAttach()
		{
			ImGui::CreateContext();
			ImGui::StyleColorsDark();

			ImGuiIO& io = ImGui::GetIO();
			io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
			io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

			ImGui_ImplOpenGL3_Init("#version 410");

		}
		void ImGuiLayer::OnDetatch()
		{
		}
		void ImGuiLayer::OnUpdate()
		{
			static bool show = true;
			
			ImGuiIO& io = ImGui::GetIO();
			Application& app = Application::Get();
			io.DisplaySize = ImVec2(app.GetWindow().GetWidth(), app.GetWindow().GetHeight());
			float time = (float)glfwGetTime();
			io.DeltaTime = m_Time > 0.0 ? (time - m_Time) : (1.00f / 60.f);
			m_Time = time;

			ImGui_ImplOpenGL3_NewFrame();
			ImGui::NewFrame();
			ImGui::ShowDemoWindow(&show);

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}
		void ImGuiLayer::OnEvent(Events::EventHandler& handler, Events::Event& event)
		{
		}
	}
}
