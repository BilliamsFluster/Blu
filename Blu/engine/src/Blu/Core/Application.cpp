#include "Blupch.h"
#include "Application.h"
#include "Blu/Core/Log.h"
#include "Window.h"
#include "Input.h"
#include <glad/glad.h>
#include "imgui.h"
#include "Blu/Rendering/Buffer.h"
#include "Blu/Rendering/VertexArray.h"
#include "Blu/Rendering/Renderer.h"
#include "Blu/Core/Timestep.h"
#include <GLFW/glfw3.h>





namespace Blu
{
	

	Application* Application::s_Instance = nullptr;
	
	Application::Application()
		
	{
		m_Color = { 1,1,1,1 };
		m_Window = std::unique_ptr<Window>(Window::Create());
		s_Instance = this;

		Renderer::Init();

		



		//// Create a framebuffer
		//glGenFramebuffers(1, &m_FrameBufferObject);
		//glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBufferObject);

		//// Create a texture
		//glGenTextures(1, &m_Texture);
		//glBindTexture(GL_TEXTURE_2D, m_Texture);
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 800, 600, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr); // change 800x600 to your window size

		//// Set texture parameters
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		//// Attach it to the framebuffer
		//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Texture, 0);

		//// Check if framebuffer is complete
		//if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		//	std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

		//// Unbind to reset to default framebuffer
		//glBindFramebuffer(GL_FRAMEBUFFER, 0);

		//std::cout << "OpenGl Error: " << glGetError() << std::endl;
		// no 1281 error here but error 0 here

		
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
		float time = glfwGetTime(); // need platform class Platform::GetTime()
		Timestep timestep = time - m_LastFrameTime; // get delta time
		m_LastFrameTime = time;
		
		while (m_Running)
		{
			m_Window->OnUpdate();
			m_Running = !m_Window->ShouldClose();



			for (Layers::Layer* layer : m_LayerStack)
			{
				layer->OnUpdate(timestep);
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