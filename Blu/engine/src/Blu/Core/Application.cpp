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
#include "Blu/Events/WindowEvent.h"
#include "Blu//Scripting/ScriptEngine.h"







namespace Blu
{
	static void CheckOpenGLError()
	{
		GLenum err;
		while ((err = glGetError()) != GL_NO_ERROR)
		{
			BLU_CORE_ERROR("Error {0}", err);

		}
	}

	Application* Application::s_Instance = nullptr;
	
	
	bool Application::IsMaximized() const
	{
		return (bool)glfwGetWindowAttrib((GLFWwindow*)&m_Window, GLFW_MAXIMIZED);
	}


	Application::Application(const std::string& name)
		
	{
		BLU_PROFILE_FUNCTION();
		
		
		// Initialize the application's window and ImGui layer
		m_Window = std::unique_ptr<Window>(Window::Create(WindowProps(name)));
		m_ImGuiLayer = std::make_shared<Layers::ImGuiLayer>();
		s_Instance = this;

		// Initialize the renderer and push the ImGui layer
		Renderer::Init();
		ScriptEngine::Init();
		PushOverlay(m_ImGuiLayer);
		GLenum error = glGetError();  // Consume any existing errors
		if (error != GL_NO_ERROR) {
			std::cout << "OpenGL error Application::Application: " << error << std::endl;
		}
		
		

	}
	
	Application::~Application()
	{
		ScriptEngine::Shutdown();
		//need Renderer::Shutdown();
	}

	// Push a new layer into the application
	void Application::PushLayer(Shared<Layers::Layer> layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
		GLenum error = glGetError();  // Consume any existing errors
		if (error != GL_NO_ERROR) {
			std::cout << "OpenGL error Application::PushLayer: " << error << std::endl;
		}
	}

	// Push a new overlay into the application
	void Application::PushOverlay(Shared<Layers::Layer> overlay)
	{
		m_LayerStack.PushOverlay(overlay);
		overlay->OnAttach();

	}

	// Close the application
	void Application::Close()
	{
		m_Running = false;
	}

	// Run the application
	void Application::Run()
	{
		BLU_PROFILE_FUNCTION();
		// Initialize the time and timestep variables
		float time = glfwGetTime(); // need platform class Platform::GetTime()
		Timestep timestep = time - m_LastFrameTime; // get delta time
		m_LastFrameTime = time;
		
		// Main application loop
		while (m_Running)
		{
			// If the window is valid we can update it 
			if (m_Window)
			{
				BLU_PROFILE_SCOPE("RunLoop");
				
				// Update the window and check if it should be closed
				m_Window->OnUpdate();
				m_Running = !m_Window->ShouldClose();

				// Update all layers
				for (Shared<Layers::Layer> layer : m_LayerStack)
				{
					BLU_PROFILE_SCOPE("LayerStack OnUpdates");
					layer->OnUpdate(timestep);
				}
				// Begin ImGui layer and draw dockspace
				m_ImGuiLayer->Begin();
				m_ImGuiLayer->DrawDockspace();

				// Draw GUI for all layers
				for (Shared<Layers::Layer> layer : m_LayerStack)
				{
					BLU_PROFILE_SCOPE("LayerStack OnUpdates");
					layer->OnGuiDraw();
				}

				// End ImGui layer
				m_ImGuiLayer->End();
			}
			
		}
		
		
	}
	void Application::OnEvent(Events::Event& event)
	{ 
		Events::EventHandler handler;
		event.Accept(handler);
		
	}
	

	
}