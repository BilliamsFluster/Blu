#pragma once
#include "Core.h"
#include "Window.h"
#include "Blu/Core/LayerStack.h"
#include "Blu/Events/EventDispatcher.h"
#include "Blu/Rendering/OrthographicCamera.h"
#include "Blu/ImGui/ImGuiLayer.h"
#include <vector>
#include <string>



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
		bool IsMaximized() const;

		// Command-line arguments captured in main() before CreateApplication() runs,
		// so layers can read them during OnAttach (e.g. a startup scene override).
		static const std::vector<std::string>& GetCommandLineArgs() { return s_CommandLineArgs; }
		static void SetCommandLineArgs(int argc, char** argv)
		{
			s_CommandLineArgs.clear();
			for (int i = 0; i < argc; ++i)
				s_CommandLineArgs.emplace_back(argv[i]);
		}

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
		static std::vector<std::string> s_CommandLineArgs;
	};
	
	//needs to be defined in client
	Application* CreateApplication();
}