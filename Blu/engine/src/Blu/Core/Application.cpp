#include "Blupch.h"
#include "Application.h"
#include "Blu/Core/Log.h"
#include "Window.h"
#include "Input.h"
#include <glad/glad.h>





namespace Blu
{
	Application* Application::s_Instance = nullptr;
	Application::Application()
	{

		m_Window = std::unique_ptr<Window>(Window::Create());
		s_Instance = this;
		unsigned int id;

		glGenVertexArrays(1, &id);
		
	}

	Application::~Application()
	{

	}

	void Application::PushLayer(Layers::Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layers::Layer* overlay)
	{
		m_LayerStack.PushOverlay(overlay);
		overlay->OnAttach();

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
			auto [x, y] = WindowInput::Input::GetMousePosition();

		}
		
		
	}
	void Application::OnEvent(Events::Event& event)
	{
		//// Dispatch the event to the layer stack
		//m_EventDispatcher.Dispatch(*this, event);

		//// If the event is a window close event, stop the application
		//if (event.GetType() == Events::Event::Type::WindowClose)
		//	m_Running = false;
	}

	
}