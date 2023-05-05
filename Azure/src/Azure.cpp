#include <Blu.h>
#include "Blu/Core/EntryPoint.h"

class Azure : public Blu::Application
{
public:
	Azure()
	{


	}
	~Azure()
	{

	}
};

Blu::Application* Blu::CreateApplication()
{
	return new Azure();
}