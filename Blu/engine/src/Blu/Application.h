#pragma once
#include "Core.h"
namespace Blu
{
	class BLU_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();
	};
	
	//needs to be defined in client
	Application* CreateApplication();
}