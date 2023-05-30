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

		glGenVertexArrays(1, &m_VertexArray);
		glBindVertexArray(m_VertexArray);

		glGenBuffers(1, &m_VertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);

		float vertices[3 * 3] = {
			-0.8f, -0.1f, 0.0f,
			-0.1f, -0.1f, 0.0f,
			-0.4f, -1.f, 0.0f
		};
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

		glGenBuffers(1, &m_IndexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexBuffer);

		unsigned int indicies[3] = { 0, 1, 2 };
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indicies), indicies, GL_STATIC_DRAW); 
		
		// Create a framebuffer
		glGenFramebuffers(1, &m_FrameBufferObject);
		glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBufferObject);

		// Create a texture
		glGenTextures(1, &m_Texture);
		glBindTexture(GL_TEXTURE_2D, m_Texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 800, 600, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr); // change 800x600 to your window size

		// Set texture parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Attach it to the framebuffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Texture, 0);

		// Check if framebuffer is complete
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

		// Unbind to reset to default framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
			glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBufferObject);
			glBindVertexArray(m_VertexArray);
			glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
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