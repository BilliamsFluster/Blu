#include "Blupch.h"
#include "Application.h"
#include "Blu/Core/Log.h"
#include "Window.h"
#include "Application.h"




namespace Blu
{
	Application::Application()
	{
		m_Window = std::unique_ptr<Window>(Window::Create());
		s_Instance = this;
		
	}

	Application::~Application()
	{

	}

	void Application::PushLayer(Layers::Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
	}

	void Application::PushOverlay(Layers::Layer* overlay)
	{
		m_LayerStack.PushOverlay(overlay);
	}

	void Application::Run()
	{
		
		
		while (m_Running)
		{
			m_Window->OnUpdate();
			m_Running = !m_Window->ShouldClose();
			for (Layers::Layer* layer : m_LayerStack)
			{
				layer->OnUpdate();
			}
		}

		
	}
	void Application::OnEvent(Blu::Events::Event& event)
	{
		////m_EventDispatcher.Dispatch(event); // dispatch event
		//// Iterate over all layers from top to bottom (assuming topmost layer is at the end of the list)
		//for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		//{
		//	// Pass the event to the current layer
		//	(*it)->OnEvent(event);

		//	// If the event was handled by the layer, stop propagation to lower layers
		//	if (event.Handled)
		//		break;
		//}
	}
	Application* Application::s_Instance = nullptr;
}