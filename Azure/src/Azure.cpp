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
		BLU_INFO("ExampleLayer::Update");
	} 

	void OnEvent(Blu::Events::Event& event) override
	{
		BLU_TRACE("{0}", event.GetType()); // causes an error 
	}
};


class Azure : public Blu::Application
{
public:
	Azure()
	{
		PushLayer(new ExampleLayer());

	}
	~Azure()
	{

	}
};

Blu::Application* Blu::CreateApplication()
{
	return new Azure();
}