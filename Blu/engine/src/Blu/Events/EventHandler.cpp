#pragma once
#include "Blupch.h"
#include "KeyEvent.h"
#include "MouseEvent.h"
#include "EventHandler.h"
#include "Blu/Core/Log.h"
#include "Blu/Core/KeyCodes.h"
#include "Blu/Events/WindowEvent.h"
#include "Blu/Core/Application.h"

namespace Blu
{
	namespace Events
	{
		void KeyPressedEventHandler::HandleEvent(Event& event)
		{
			BLU_CORE_TRACE("KeyPressed");
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
			BLU_CORE_TRACE(" Mouse X Offset:{0} Mouse Y Offset: {1} ", MouseEvent.GetXOffset(), MouseEvent.GetYOffset());
		}
		
		void MouseButtonPressedEventHandler::HandleEvent(Event& event)
		{
			MouseButtonPressedEvent MouseEvent = dynamic_cast<MouseButtonPressedEvent&>(event);
			BLU_CORE_TRACE("Mouse Pressed");

			
			
			
			
		}
		void MouseButtonReleasedEventHandler::HandleEvent(Event& event)
		{
			MouseButtonReleasedEvent MouseEvent = dynamic_cast<MouseButtonReleasedEvent&>(event);
			BLU_CORE_TRACE("Mouse Released");

		}
		void KeyReleasedEventHandler::HandleEvent(Event& event)
		{
			KeyReleasedEvent MouseEvent = dynamic_cast<KeyReleasedEvent&>(event);
			BLU_CORE_TRACE("Key Released");
		}
		void WindowResizeEventHandler::HandleEvent(Event& event)
		{
			WindowResizeEvent WindowEvent = dynamic_cast<WindowResizeEvent&>(event);
			BLU_CORE_TRACE("Window Size Y: {0} X: {1} ",WindowEvent.GetHeight(), WindowEvent.GetWidth());
		}
		void KeyTypedEventHandler::HandleEvent(Event& event)
		{
			BLU_CORE_TRACE("KeyTyped");;
		}
	}
	

}
