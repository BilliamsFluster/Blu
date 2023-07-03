#pragma once
#include "Core.h"
#include "Window.h"
#include "Blu/Core/LayerStack.h"
#include "Blu/Events/EventDispatcher.h"
#include "Blu/Platform/OpenGL/OpenGLShader.h"
#include "Blu/Rendering/OrthographicCamera.h"
#include "Blu/ImGui/ImGuiLayer.h"



struct Vec4
{
	float x, y, z, w;
};
namespace Blu
{
	class BLU_API Application
	{
	public:

		

		Application(const std::string& name = "Blu Engine");
		
		virtual ~Application();
		void PushLayer(Shared<Layers::Layer> layer);
		void PushOverlay(Shared<Layers::Layer> overlay);
		void Run();
		void Close(); 
		void OnEvent(Events::Event& event);
		Shared<Layers::ImGuiLayer> GetImGuiLayer() { return m_ImGuiLayer; }
		Events::EventDispatcher& GetEventDispatcher() { return m_EventDispatcher; }
		Layers::LayerStack& GetLayerStack() { return m_LayerStack; }

		inline static Application& Get() { return *s_Instance; }
		inline Window& GetWindow() { return *m_Window; }

	private:
		Unique<Window> m_Window;
		Shared<Layers::ImGuiLayer> m_ImGuiLayer;
		bool m_Running = true;
		Layers::LayerStack m_LayerStack;
		Events::EventDispatcher m_EventDispatcher;
		unsigned int m_Texture, m_FrameBufferObject;
		Vec4 m_Color;

		float m_LastFrameTime = 0.0f;
	private:
		static Application* s_Instance;
		
	};
	
	//needs to be defined in client
	Application* CreateApplication();
}