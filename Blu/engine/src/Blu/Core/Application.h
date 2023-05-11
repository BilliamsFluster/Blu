#pragma once
#include "Core.h"
#include "Window.h"
#include "Blu/Core/LayerStack.h"

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
	private:
		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
		Layers::LayerStack m_LayerStack;
	};
	
	//needs to be defined in client
	Application* CreateApplication();
}