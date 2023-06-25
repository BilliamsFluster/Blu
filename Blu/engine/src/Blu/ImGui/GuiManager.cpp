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
	// Call ImGui::Text with the given parameters
	void GuiManager::Text(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		ImGui::TextV(fmt, args);
		va_end(args);
	}
	bool GuiManager::Button(const std::string& label, const ImVec2& size)
	{
		return ImGui::Button(label.c_str(), size);

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
}