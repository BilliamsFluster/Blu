#include <Blu.h>
#include "Blu/Core/EntryPoint.h"

class ExampleLayer : public Blu::Layers::Layer
{
public:
	ExampleLayer()
		:Layer("Example")
	{

	}
	void OnUpdate() override

	{
		//BLU_INFO("ExampleLayer::Update");
	} 

	void OnEvent(Blu::Events::EventHandler& handler, Blu::Events::Event& event) override
	{
		handler.HandleEvent(event);
		event.Handled = true;
	}
}; 


class Example2Layer : public Blu::Layers::Layer
{
public:
	Example2Layer()
		:Layer("Example2")
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
		PushLayer(new ExampleLayer());
		PushLayer(new Example2Layer());
		

	}
	~Azure()
	{

	}
};

Blu::Application* Blu::CreateApplication()
{
	return new Azure();
}