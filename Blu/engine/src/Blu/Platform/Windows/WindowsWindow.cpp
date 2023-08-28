#include "Blupch.h"
#include "WindowsWindow.h"
#include <glad/glad.h>
#include "Blu/Core/Log.h"
#include "Blu/Events/GLFWCallbacks.h"
#include "Blu/Platform/OpenGL/OpenGLContext.h"

#include "imgui.h"
#include "imgui_internal.h"



namespace Blu
{
	static bool s_GLFWInitialized = false;

	Window* Window::Create(const WindowProps& props)
	{
		
		return new WindowsWindow(props);
	}

	Blu::WindowsWindow::WindowsWindow(const WindowProps& props)
		:m_WindowProps(props)
	{
		
		BLU_PROFILE_FUNCTION();
		Init(props); // called once
	}
	bool Blu::WindowsWindow::ShouldClose() const {
		
		return glfwWindowShouldClose(m_Window);
	}

	Blu::WindowsWindow::~WindowsWindow()
	{
		Shutdown();
	}

	void Blu::WindowsWindow::OnUpdate()
	{
		BLU_PROFILE_FUNCTION();
		glfwPollEvents();
		m_Context->SwapBuffers();
	}

	void Blu::WindowsWindow::Init(const WindowProps& props)
	{
		BLU_PROFILE_FUNCTION();
		// make sure to check if we have  custom title bar
		m_Data.Title = props.Title;
		m_Data.Width = props.Width;
		m_Data.Height = props.Height;
		BLU_CORE_INFO("Creating window {0} ({1}, {2})", props.Title, props.Width, props.Height);
		if (!s_GLFWInitialized)
		{
			int success = glfwInit();
			BLU_CORE_ASSERT(success, "Could not initalize GLFW!");

			s_GLFWInitialized = true;
		}
		if (props.CustomTitleBar)
		{
			glfwWindowHint(GLFW_TITLEBAR, false);
		}
		m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, m_Data.Title.c_str(), nullptr, nullptr);
		m_Context = new OpenGLContext(m_Window);
		m_Context->Init();
		

		glfwSetWindowUserPointer(m_Window, &m_Data);
		glfwSetTitlebarHitTestCallback(m_Window, [](GLFWwindow* window, int x, int y, int* hit)
			{
				Application* app = (Application*)glfwGetWindowUserPointer(window);
				WindowsWindow* activeWindow = (WindowsWindow*)Application::Get().GetWindow().GetNativeWindow();
				*hit = activeWindow->IsTitleBarHovered();
			});

		glfwSetKeyCallback(m_Window, GLFWCallbacks::KeyCallback);
		glfwSetMouseButtonCallback(m_Window, GLFWCallbacks::MouseButtonCallback);
		glfwSetWindowSizeCallback(m_Window, GLFWCallbacks::WindowSizeCallback);
		glfwSetCursorPosCallback(m_Window, GLFWCallbacks::MouseMovedCallback);
		glfwSetScrollCallback(m_Window, GLFWCallbacks::MouseButtonScrolledCallback);
		glfwSetWindowSizeCallback(m_Window, GLFWCallbacks::WindowSizeCallback);
		glfwSetCharCallback(m_Window, GLFWCallbacks::CharCallback);


	}

	void Blu::WindowsWindow::Shutdown()
	{
		BLU_PROFILE_FUNCTION();
		glfwDestroyWindow(m_Window);
		delete m_Context;
	}

	

}