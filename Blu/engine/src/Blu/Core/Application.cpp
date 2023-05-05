#include "Application.h"
#include "Blu/Core/Log.h"
#include "Window.h"
#include "Blupch.h"
#include "Application.h"
#include "Blu/Events/EventManager.h"
#include "Blu/Events/Event.h"
#include "Blu/Events/EventHandler.h"
#include "Blu/Events/WindowEvent.h"
#include "Blu/Events/MouseEvent.h"




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
			
		}

		
	}
}