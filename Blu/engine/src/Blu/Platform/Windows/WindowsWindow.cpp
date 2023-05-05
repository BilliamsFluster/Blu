

#include "Blupch.h"
#include "WindowsWindow.h"
#include "Blu.h"

namespace Blu
{
	static bool s_GLFWInitialized = false;

	Window* Window::Create(const WindowProps& props)
	{
		return new WindowsWindow(props);
	}

	Blu::WindowsWindow::WindowsWindow(const WindowProps& props)
	{
		Init(props);
	}

	Blu::WindowsWindow::~WindowsWindow()
	{
		Shutdown();
	}

	void Blu::WindowsWindow::OnUpdate()
	{
		glfwPollEvents();
		glfwSwapBuffers(m_Window);
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
		glfwMakeContextCurrent(m_Window);
		glfwSetWindowUserPointer(m_Window, &m_Data);

	}

	void Blu::WindowsWindow::Shutdown()
	{
		glfwDestroyWindow(m_Window);
	}

}