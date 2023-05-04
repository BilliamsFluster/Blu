#pragma once
#include "Blu/Core/Core.h"
#include "Blu/Events/Event.h"
#include <iostream>
#include <string>
namespace Blu
{
	class BLU_API Window
	{
	public:
		Window(int width, int height, const std::string& title);
		~Window();
		bool pollEvent(Event& event);
		

	};
	
}