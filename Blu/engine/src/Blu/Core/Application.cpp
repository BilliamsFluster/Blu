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
#include "Blu/ImGui/GuiManager.h"







namespace Blu
{
	

	Application* Application::s_Instance = nullptr;
	
	Application::Application(const std::string& name)
		
	{
		BLU_PROFILE_FUNCTION();
		m_Color = { 1,1,1,1 };
		m_Window = std::unique_ptr<Window>(Window::Create(WindowProps(name)));
		m_ImGuiLayer = std::make_shared<Layers::ImGuiLayer>();
		s_Instance = this;

		
		Renderer::Init();
		PushOverlay(m_ImGuiLayer);

		
		

	}
	
	Application::~Application()
	{
		GuiManager::Shutdown();
	}

	void Application::PushLayer(Shared<Layers::Layer> layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Shared<Layers::Layer> overlay)
	{
		m_LayerStack.PushOverlay(overlay);
		overlay->OnAttach();

	}

	void Application::Run()
	{
		BLU_PROFILE_FUNCTION();

		float time = glfwGetTime(); // need platform class Platform::GetTime()
		Timestep timestep = time - m_LastFrameTime; // get delta time
		m_LastFrameTime = time;
		
		while (m_Running)
		{
			BLU_PROFILE_SCOPE("RunLoop");
			m_Window->OnUpdate();
			m_Running = !m_Window->ShouldClose();

			for (Shared<Layers::Layer> layer : m_LayerStack)
			{
				{
					BLU_PROFILE_SCOPE("LayerStack OnUpdates");
					layer->OnUpdate(timestep);
				}
			}
			m_ImGuiLayer->Begin();
			m_ImGuiLayer->DrawDockspace();
			for (Shared<Layers::Layer> layer : m_LayerStack)
			{
				{
					BLU_PROFILE_SCOPE("LayerStack OnUpdates");
					
					layer->OnGuiDraw();
				}
			}
			m_ImGuiLayer->End();

			
			
			auto [x, y] = Blu::Input::GetMousePosition();

			
		}
		
		
	}
	void Application::OnEvent(Events::Event& event)
	{ 
		Events::EventHandler handler;
		event.Accept(handler);
		
	}

	
}