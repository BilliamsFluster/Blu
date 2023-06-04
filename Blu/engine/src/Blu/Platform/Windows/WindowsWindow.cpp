#include "Blupch.h"
#include "WindowsWindow.h"
#include "Blu/Core/Log.h"
#include "Blu/Events/GLFWCallbacks.h"
#include "Blu/Platform/OpenGL/OpenGLContext.h"
#include <glad/glad.h>

namespace Blu
{
	static bool s_GLFWInitialized = false;

	Window* Window::Create(const WindowProps& props)
	{
		return new WindowsWindow(props);
	}

	Blu::WindowsWindow::WindowsWindow(const WindowProps& props)
	{
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
		glfwPollEvents();
		m_Context->SwapBuffers();
	}

	void Blu::WindowsWindow::Init(const WindowProps& props)
	{
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
		m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, m_Data.Title.c_str(), nullptr, nullptr);
		m_Context = new OpenGLContext(m_Window);
		m_Context->Init();
		
		BLU_CORE_ASSERT(status, "Failed to init Glad");

		glfwSetWindowUserPointer(m_Window, &m_Data);

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
		glfwDestroyWindow(m_Window);
	}

}