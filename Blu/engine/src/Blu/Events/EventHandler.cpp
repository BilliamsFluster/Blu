#pragma once
#include "Blupch.h"
#include "KeyEvent.h"
#include "MouseEvent.h"
#include "EventHandler.h"
#include "Blu/Core/KeyCodes.h"
#include "Blu/Events/WindowEvent.h"

namespace Blu
{
	namespace Events
	{
		void KeyPressedEventHandler::HandleEvent(Event& event)
		{

			KeyPressedEvent& keyEvent = dynamic_cast<KeyPressedEvent&>(event);
			
		}

		void MouseMovedEventHandler::HandleEvent(Event& event)
		{
			MouseMovedEvent& MouseEvent = dynamic_cast<MouseMovedEvent&>(event);

			//std::cout << "Mouse Pos X: "<<  MouseEvent.GetX() <<" Mouse Pos Y: "<< MouseEvent.GetY() <<std::endl;


		}
		void MouseScrolledEventHandler::HandleEvent(Event& event)
		{
			MouseScrolledEvent MouseEvent = dynamic_cast<MouseScrolledEvent&>(event);
			std::cout << " Mouse X Offset: " << MouseEvent.GetXOffset() <<   " Mouse Y Offset:" << MouseEvent.GetYOffset()  << std::endl;
		}
		void MouseButtonPressedEventHandler::HandleEvent(Event& event)
		{
			MouseButtonPressedEvent MouseEvent = dynamic_cast<MouseButtonPressedEvent&>(event);
			std::cout << "Mouse Pressed" << std::endl;
			
			
		}
		void MouseButtonReleasedEventHandler::HandleEvent(Event& event)
		{
			MouseButtonReleasedEvent MouseEvent = dynamic_cast<MouseButtonReleasedEvent&>(event);
			std::cout << "Mouse Released" << std::endl;

		}
		void KeyReleasedEventHandler::HandleEvent(Event& event)
		{
			KeyReleasedEvent MouseEvent = dynamic_cast<KeyReleasedEvent&>(event);
			std::cout << "Key Released" << std::endl;
		}
		void WindowResizeEventHandler::HandleEvent(Event& event)
		{
			WindowResizeEvent WindowEvent = dynamic_cast<WindowResizeEvent&>(event);
			std::cout << "Window Size: " << "Y: " << WindowEvent.GetHeight() << " X:" << WindowEvent.GetWidth() <<std::endl;
		}
		void KeyTypedEventHandler::HandleEvent(Event& event)
		{
			std::cout << "KeyTyped" << std::endl;
		}
	}
	

}
