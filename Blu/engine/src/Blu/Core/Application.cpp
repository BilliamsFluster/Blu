#include "Application.h"
#include "Blu/Core/Log.h"
#include "Window.h"
#include "Blu/Events/EventManager.h"
#include "Blu/Events/Event.h"
#include "Blu/Events/EventHandler.h"
#include "Blu/Events/WindowEvent.h"

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
		Window window(...);

		EventManager Manager;
		WindowEventHandler Handler;

		Manager.AddEventHandler(&Handler);
		
		while (true)
		{
			WindowResizeEvent event(1280,720);
			while (window.pollEvent(event))
			{
				// handle event
				Manager.HandleEvent(event);
			}
		}

		
	}
}