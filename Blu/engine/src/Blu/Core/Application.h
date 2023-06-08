#pragma once
#include "Core.h"
#include "Window.h"
#include "Blu/Core/LayerStack.h"
#include "Blu/Events/EventDispatcher.h"
#include "Blu/Platform/OpenGL/OpenGLShader.h"
#include "Blu/Rendering/OrthographicCamera.h"


struct Vec4
{
	float x, y, z, w;
};
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
	private: // dont make public

		unsigned int m_Texture, m_FrameBufferObject;
		
		
		


		Vec4 m_Color;
	private:
		static Application* s_Instance;
		
	};
	
	//needs to be defined in client
	Application* CreateApplication();
}