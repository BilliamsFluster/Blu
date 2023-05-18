#include <Blu.h>
#include "Blu/Core/EntryPoint.h"

class Rendering : public Blu::Layers::Layer
{
public:
	Rendering()
		:Layer("Rendering")
	{

	}
	void OnUpdate() override
	{
	} 

	void OnEvent(Blu::Events::EventHandler& handler, Blu::Events::Event& event) override
	{
		handler.HandleEvent(event);
		event.Handled = true;
	}
}; 


class Engine : public Blu::Layers::Layer
{
public:
	Engine()
		:Layer("Engine")
	{

	}
	void OnUpdate() override

	{
		
	}

	void OnEvent(Blu::Events::EventHandler& handler, Blu::Events::Event& event) override
	{

	}
};


class Azure : public Blu::Application
{
public:
	Azure()
	{
		PushLayer(new Rendering());//1st layer
		PushLayer(new Engine()); //2nd layer 
		PushOverlay(new Blu::Layers::ImGuiLayer());
		

	}
	~Azure()
	{

	}
};

Blu::Application* Blu::CreateApplication()
{
	return new Azure();
}