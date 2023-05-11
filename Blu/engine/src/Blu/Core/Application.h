#pragma once
#include "Core.h"
#include "Window.h"
#include "Blu/Core/LayerStack.h"
#include "Blu/Events/EventDispatcher.h"


namespace Blu
{
	class BLU_API Application
	{
	public:

		static Application& Get()
		{
			return *s_Instance;
		}

		Application();
		
		virtual ~Application();
		void PushLayer(Layers::Layer* layer);
		void PushOverlay(Layers::Layer* overlay);
		void Run();
		void OnEvent(Events::Event& event);

		Events::EventDispatcher& GetEventDispatcher() { return m_EventDispatcher; }
		Layers::LayerStack& GetLayerStack() { return m_LayerStack; }

	private:
		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
		Layers::LayerStack m_LayerStack;
		static Application* s_Instance;
		Events::EventDispatcher m_EventDispatcher;
		
	};
	
	//needs to be defined in client
	Application* CreateApplication();
}