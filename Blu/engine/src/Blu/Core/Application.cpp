#include "Application.h"
#include "Blu/Core/Log.h"
#include "Window.h"
#include "Blu/Events/EventManager.h"
#include "Blu/Events/Event.h"
#include "Blu/Events/EventHandler.h"
#include "Blu/Events/WindowEvent.h"
#include "Blu/Events/MouseEvent.h"


namespace Blu
{
	Application::Application()
	{

	}

	Application::~Application()
	{

	}

	void Application::Run()
	{
		Window window(800,600,"Blu");

		EventManager Manager;
		WindowEventHandler Handler;

		Manager.AddEventHandler(&Handler);
		
		while (true)
		{
			
			
		}

		
	}
}