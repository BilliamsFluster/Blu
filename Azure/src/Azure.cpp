#include <Blu.h>
#include <Blu/Core/EntryPoint.h>
#include "GameLayer.h"

class AzureApp : public Blu::Application
{
public:
	AzureApp() : Blu::Application("Azure")
	{
		PushLayer(std::make_shared<Azure::GameLayer>());
	}
	~AzureApp() = default;
};

Blu::Application* Blu::CreateApplication()
{
	return new AzureApp();
}
