#include <Blu.h>

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