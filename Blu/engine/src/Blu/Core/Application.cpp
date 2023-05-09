#include "Application.h"
#include "Blu/Core/Log.h"
#include "Window.h"
#include "Blupch.h"
#include "Application.h"




namespace Blu
{
	Application::Application()
	{
		m_Window = std::unique_ptr<Window>(Window::Create());
	}

	Application::~Application()
	{

	}

	void Application::Run()
	{
		
		
		while (m_Running)
		{
			m_Window->OnUpdate();
			m_Running = !m_Window->ShouldClose();
		}

		
	}
}