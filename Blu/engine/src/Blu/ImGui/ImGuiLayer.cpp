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
			ImGui::StyleColorsDark();

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

		//	}*/
		//}
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
		bool ImGuiLayer::OnWindowResizedEvent(Events::WindowResizeEvent& event)
		{
			BLU_PROFILE_FUNCTION();

			ImGuiIO& io = ImGui::GetIO();

			// Update the display size
			io.DisplaySize = ImVec2(event.GetWidth(), event.GetHeight());

			io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f); // Assuming no scale here

			

			return false;
		}
		void ImGuiLayer::OnEvent(Events::Event& event)
		{
			
			BLU_PROFILE_FUNCTION();

			ImGuiIO& io = ImGui::GetIO();
			Events::EventHandler handler;
			event.Accept(handler);
			if (m_BlockEvents) {
				return;
			}
			
				
			
			switch (event.GetType())
			{

			case Events::Event::Type::WindowResize:
			{
				Events::WindowResizeEvent& e = static_cast<Events::WindowResizeEvent&>(event);
				OnWindowResizedEvent(e);
				break;

				
			}
			case Events::Event::Type::KeyPressed:
			{
				Events::KeyPressedEvent& e = static_cast<Events::KeyPressedEvent&>(event);
				//OnKeyPressedEvent(e);
				break;

			}
			case Events::Event::Type::MouseButtonPressed:
			{
				Events::MouseButtonPressedEvent& e = static_cast<Events::MouseButtonPressedEvent&>(event);
				OnMouseButtonPressed(e);
				break;
			}
			}
		}

			

		bool ImGuiLayer::OnMouseButtonPressed(Events::MouseButtonPressedEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.MouseDown[event.GetButton()] = true;
			event.Handled = true;

			return false;



		}
		bool ImGuiLayer::OnMouseButtonReleased(Events::MouseButtonReleasedEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.MouseDown[event.GetButton()] = false;
			event.Handled = true;
			return false;


		}
		bool ImGuiLayer::OnMouseScrolledEvent(Events::MouseScrolledEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.MouseWheel = event.GetYOffset();
			io.MouseWheelH = event.GetXOffset();
			event.Handled = true;
			return false;

		}
		bool ImGuiLayer::OnMouseMovedEvent(Events::MouseMovedEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.MousePos = ImVec2(event.GetX(), event.GetY());
			event.Handled = true;
			return false;
		}

		bool ImGuiLayer::OnKeyPressedEvent(Events::KeyPressedEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.KeysDown[event.GetKeyCode()] = true;
			io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
			io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
			io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
			io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
			
			event.Handled = true;
			return false;

		}

		bool ImGuiLayer::OnKeyReleasedEvent(Events::KeyReleasedEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.KeysDown[event.GetKeyCode()] = false;
			event.Handled = true;
			return false;

		}

		bool ImGuiLayer::OnKeyTypedEvent(Events::KeyTypedEvent& event)
		{
			ImGuiIO& io = ImGui::GetIO();
			int KeyCode = event.GetKeyCode();
			if (KeyCode > 0 && KeyCode < 0x10000)
			{
				io.AddInputCharacter((unsigned short)KeyCode);
			}
			event.Handled = true;
			return false;

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

			ImGuiIO& io = ImGui::GetIO();
			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
			{
				ImGuiID dockspace_id = ImGui::GetID("BluDockspace");
				ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
			}

			// Other ImGui code here...

			ImGui::End();
		}


	}
}
