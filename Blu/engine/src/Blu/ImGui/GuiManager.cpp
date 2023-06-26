#include "Blupch.h"
#include "GuiManager.h"
#include "imgui.h"
#include "Blu/Platform/OpenGL/ImGuiOpenGLRenderer.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Blu/Core/Application.h"
#include "Blu/Events/MouseEvent.h"
#include "Blu/Events/KeyEvent.h"


namespace Blu
{
	void GuiManager::Initialize()
	{
		// Create an ImGui context
		ImGui::CreateContext();

		// Set up ImGui style
		ImGui::StyleColorsDark();

		// Set up ImGui IO
		ImGuiIO& io = ImGui::GetIO();
		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
		io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

		// Initialize ImGui for use with OpenGL
		ImGui_ImplOpenGL3_Init("#version 410");
	}
	void GuiManager::Shutdown()
	{
		// Shutdown ImGui's OpenGL renderer
		ImGui_ImplOpenGL3_Shutdown();

		// Destroy the ImGui context
		ImGui::DestroyContext();
	}

	void GuiManager::BeginFrame()
	{
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

		// Start a new ImGui frame for OpenGL
		ImGui_ImplOpenGL3_NewFrame();

		// Start a new ImGui frame
		ImGui::NewFrame();
	}
	void GuiManager::EndFrame()
	{
		// Render ImGui draw data
		ImGui::Render();

		// Render ImGui draw data using OpenGL
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
	bool GuiManager::Begin(const std::string& name, bool* open )
	{
		
		return ImGui::Begin(name.c_str(), open, 0);
	}
	void GuiManager::End()
	{
		ImGui::End();
	}

	void GuiManager::Image(unsigned int texture, const Vec2& size) {
		ImVec2 imguiSize(size.x, size.y);
		ImGui::Image((ImTextureID)(intptr_t)texture, imguiSize);
	}
	// Call ImGui::Text with the given parameters
	void GuiManager::Text(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		ImGui::TextV(fmt, args);
		va_end(args);
	}
	bool GuiManager::Button(const std::string& label, const Vec2& size)
	{
		ImVec2 imguiSize(size.x, size.y);
		return ImGui::Button(label.c_str(), imguiSize);

	}
	bool GuiManager::BeginMenu(const std::string& label, bool enabled)
	{
		return ImGui::BeginMenu(label.c_str(), enabled);
	}

	void GuiManager::EndMenu()
	{
		ImGui::EndMenu();
	}

	bool GuiManager::OnMouseMovedEvent(Events::MouseMovedEvent& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MousePos = ImVec2(event.GetX(), event.GetY());
		return false;
	}

	bool GuiManager::OnKeyPressedEvent(Events::KeyPressedEvent& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.KeysDown[event.GetKeyCode()] = true;
		io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
		io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
		io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
		io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];


		return false;

	}

	
	bool GuiManager::OnMouseButtonPressed(Events::MouseButtonPressedEvent& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDown[event.GetButton()] = true;
		return false;



	}
	bool GuiManager::OnMouseButtonReleased(Events::MouseButtonReleasedEvent& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MouseDown[event.GetButton()] = false;
		return false;


	}
	bool GuiManager::OnMouseScrolledEvent(Events::MouseScrolledEvent& event)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.MouseWheel = event.GetYOffset();
		io.MouseWheelH = event.GetXOffset();
		return false;

	}

	void GuiManager::ShowDockSpace(bool* p_open)
	{
		
		static bool opt_fullscreen = true;
		static bool opt_padding = false;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen)
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}
		else
		{
			dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
		}

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
		// and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		if (!opt_padding)
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace Demo", p_open, window_flags);
		if (!opt_padding)
			ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// Submit the DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("Options"))
			{
				// Disabling fullscreen would allow the window to be moved to the front of other windows,
				// which we can't undo at the moment without finer window depth/z control.
				ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen);
				ImGui::MenuItem("Padding", NULL, &opt_padding);
				ImGui::Separator();

				if (ImGui::MenuItem("Flag: NoSplit", "", (dockspace_flags & ImGuiDockNodeFlags_NoSplit) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoSplit; }
				if (ImGui::MenuItem("Flag: NoResize", "", (dockspace_flags & ImGuiDockNodeFlags_NoResize) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoResize; }
				if (ImGui::MenuItem("Flag: NoDockingInCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingInCentralNode) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoDockingInCentralNode; }
				if (ImGui::MenuItem("Flag: AutoHideTabBar", "", (dockspace_flags & ImGuiDockNodeFlags_AutoHideTabBar) != 0)) { dockspace_flags ^= ImGuiDockNodeFlags_AutoHideTabBar; }
				if (ImGui::MenuItem("Flag: PassthruCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) != 0, opt_fullscreen)) { dockspace_flags ^= ImGuiDockNodeFlags_PassthruCentralNode; }
				ImGui::Separator();

				if (ImGui::MenuItem("Close", NULL, false, p_open != NULL))
					*p_open = false;
				ImGui::EndMenu();
			}
			

			ImGui::EndMenuBar();
		}

		ImGui::End();
	}
}