#include "Blupch.h"
#include "ImGuiLayer.h"
#include "Blu/Core/Log.h"
#include "Blu/Platform/OpenGL/ImGuiOpenGLRenderer.h"
#include <GLFW/glfw3.h>
#include "Blu/Core/Application.h"
#include "Blu/Events/EventDispatcher.h"
#include "Blu/Rendering/Renderer2D.h"
#include <imgui.h>



//Temporary

#include <GLFW/glfw3.h>
#include <glad/glad.h>

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
			BLU_PROFILE_FUNCTION();
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			//ImGui::StyleColorsDark();
			SetDarkColors();


			ImGuiIO& io = ImGui::GetIO();
			Application& app = Application::Get();
			GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
			// Get the framebuffer size
			int FrameBufferWidth, FrameBufferHeight;
			glfwGetFramebufferSize(window, &FrameBufferWidth, &FrameBufferHeight);

			// Get the window size
			int WindowWidth, WindowHeight;
			glfwGetWindowSize(window, &WindowWidth, &WindowHeight);

			io.DisplaySize = ImVec2((float)FrameBufferWidth, (float)FrameBufferHeight);
			float time = (float)glfwGetTime();
			static float m_Time = 0.0f;
			io.DeltaTime = m_Time > 0.0 ? (time - m_Time) : (1.00f / 60.f);
			m_Time = time;
			io.Fonts->AddFontFromFileTTF("assets/fonts/opensans/static/OpenSans-Bold.ttf", 18.0f);

			io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/opensans/static/OpenSans-Light.ttf", 18.0f);
			
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
			io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
			
			ImGuiStyle& style = ImGui::GetStyle();
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				style.WindowRounding = 0.0f;
				style.Colors[ImGuiCol_WindowBg].w = 1.0f;
			}
			//ImGui_ImplGlfw_InitForOpenGL(window, true);
			ImGui_ImplOpenGL3_Init("#version 410");

		}
		void ImGuiLayer::OnDetach()
		{
			ImGui_ImplOpenGL3_Shutdown();
			ImGui::DestroyContext();
		}

		void ImGuiLayer::OnGuiDraw()
		{
			ImGuiIO& io = ImGui::GetIO();
			GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
			
			
			float time = (float)glfwGetTime();
			static float m_Time = 0.0f;
			io.DeltaTime = m_Time > 0.0 ? (time - m_Time) : (1.00f / 60.f);
			m_Time = time;
			static bool show = true;
			ImGui::ShowDemoWindow(&show);
		}

		void ImGuiLayer::SetDarkColors()
		{
			auto& colors = ImGui::GetStyle().Colors;
			colors[ImGuiCol_WindowBg] =				ImVec4{0.05f, 0.055f, 0.05f, 1.0f};
			
			/* Header */
			colors[ImGuiCol_Header] =				ImVec4{ 0.2f, 0.2f, 0.2f, 1.0f };
			colors[ImGuiCol_HeaderHovered] =		ImVec4{ 0.3f, 0.3f, 0.3f, 1.0f };
			colors[ImGuiCol_HeaderActive] =			ImVec4{ 0.25f, 0.25f, 0.25f, 1.0f };
			
			/* Button */
			colors[ImGuiCol_Button] =				ImVec4{0.15f, 0.15f, 0.15f, 1.0f};
			colors[ImGuiCol_ButtonHovered] =		ImVec4{0.25f, 0.25f, 0.25f, 1.0f};
			colors[ImGuiCol_ButtonActive] =			ImVec4{0.2f, 0.2f, 0.2f, 1.0f};
			
			/* Frame */
			colors[ImGuiCol_FrameBg] =				ImVec4{ 0.1f, 0.1f, 0.1f, 1.0f };
			colors[ImGuiCol_FrameBgHovered] =		ImVec4{ 0.2f, 0.2f, 0.2f, 1.0f };
			colors[ImGuiCol_FrameBgActive] =		ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };
			
			/* Tab */
			colors[ImGuiCol_Tab] =					ImVec4{0.06f, 0.066f, 0.06f, 1.0f};
			colors[ImGuiCol_TabHovered] =			ImVec4{0.07f, 0.077f, 0.07f, 1.0f};
			colors[ImGuiCol_TabActive] =			ImVec4{ 0.1f, 0.1f, 0.1f, 1.0f };
			colors[ImGuiCol_TabUnfocused] =			ImVec4{0.068f, 0.068f, 0.068f, 1.0f};
			colors[ImGuiCol_TabUnfocusedActive] =	ImVec4{0.078f, 0.078f, 0.078f, 1.0f};
			
			/* Title */
			colors[ImGuiCol_TitleBg] =				ImVec4{ 0.1f, 0.1f, 0.1f, 1.0f };
			colors[ImGuiCol_TitleBgActive] =		ImVec4{ 0.2f, 0.2f, 0.2f, 1.0f };
			colors[ImGuiCol_TitleBgCollapsed] =		ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };
						
			/* Text */
			colors[ImGuiCol_Text] =					ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f }; 
			colors[ImGuiCol_Border] =				ImVec4{0.14f, 0.16f, 0.15f, 0.88f};
			colors[ImGuiCol_ScrollbarBg] =			ImVec4{0.02f, 0.02f, 0.02f, 0.8f};
														  
		}												  

		

		
		void ImGuiLayer::Begin()
		{
			ImGui_ImplOpenGL3_NewFrame();
			//ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
		}
		void ImGuiLayer::End()
		{
			ImGuiIO& io = ImGui::GetIO();
			Application& app = Application::Get();
			//io.DisplaySize = ImVec2(app.GetWindow().GetWidth(), app.GetWindow().GetHeight());

			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				GLFWwindow* backup_current_context = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backup_current_context);
			}
		}
		
		
		void ImGuiLayer::DrawDockspace()
		{
			
			static bool dockspaceOpen = true;
			static bool opt_fullscreen = true;
			static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

			ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
			if (opt_fullscreen)
			{
				ImGuiViewport* viewport = ImGui::GetMainViewport();
				ImGui::SetNextWindowPos(viewport->Pos);
				ImGui::SetNextWindowSize(viewport->Size);
				ImGui::SetNextWindowViewport(viewport->ID);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
				window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
				window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
			}

			if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
				window_flags |= ImGuiWindowFlags_NoBackground;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("Blu Dockspace", &dockspaceOpen, window_flags);
			ImGui::PopStyleVar();

			if (opt_fullscreen)
				ImGui::PopStyleVar(2);

			ImGuiStyle& style = ImGui::GetStyle();
			float minWinSizeX = style.WindowMinSize.x;
			style.WindowMinSize.x = 350.0f;
			ImGuiIO& io = ImGui::GetIO();
			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
			{
				ImGuiID dockspace_id = ImGui::GetID("BluDockspace");
				ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
			}

			style.WindowMinSize.x = minWinSizeX;

			ImGui::End();
		}


	}
}
