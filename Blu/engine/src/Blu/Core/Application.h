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

		

		Application();
		
		virtual ~Application();
		void PushLayer(Layers::Layer* layer);
		void PushOverlay(Layers::Layer* overlay);
		void Run();
		void OnEvent(Events::Event& event);

		Events::EventDispatcher& GetEventDispatcher() { return m_EventDispatcher; }
		Layers::LayerStack& GetLayerStack() { return m_LayerStack; }

		inline static Application& Get() { return *s_Instance; }
		inline Window& GetWindow() { return *m_Window; }

	private:
		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
		Layers::LayerStack m_LayerStack;
		Events::EventDispatcher m_EventDispatcher;
	private:
		static Application* s_Instance;
		
	};
	
	//needs to be defined in client
	Application* CreateApplication();
}