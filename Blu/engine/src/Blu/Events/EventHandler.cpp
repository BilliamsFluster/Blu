#pragma once
#include "Blupch.h"
#include "KeyEvent.h"
#include "MouseEvent.h"
#include "EventHandler.h"
#include "Blu/Core/Log.h"
#include "Blu/Core/KeyCodes.h"
#include "Blu/Events/WindowEvent.h"
#include "Blu/Core/Application.h"
#include "Blu/Rendering/Renderer.h"

namespace Blu
{
	namespace Events
	{
		void EventHandler::HandleEvent(KeyPressedEvent& event)
		{
			BLU_CORE_TRACE("KeyPressed");
			KeyPressedEvent& keyEvent = dynamic_cast<KeyPressedEvent&>(event);
			
			
		
			
		}

		void EventHandler::HandleEvent(MouseMovedEvent& event)
		{
			MouseMovedEvent& MouseEvent = dynamic_cast<MouseMovedEvent&>(event);

			//std::cout << "Mouse Pos X: "<<  MouseEvent.GetX() <<" Mouse Pos Y: "<< MouseEvent.GetY() <<std::endl;


		}
		void EventHandler::HandleEvent(MouseScrolledEvent& event)
		{
			MouseScrolledEvent MouseEvent = dynamic_cast<MouseScrolledEvent&>(event);
			BLU_CORE_TRACE(" Mouse X Offset:{0} Mouse Y Offset: {1} ", MouseEvent.GetXOffset(), MouseEvent.GetYOffset());
		}
		
		void EventHandler::HandleEvent(MouseButtonPressedEvent& event)
		{
			MouseButtonPressedEvent MouseEvent = dynamic_cast<MouseButtonPressedEvent&>(event);
			BLU_CORE_TRACE("Mouse Pressed");

			
			
			
			
		}
		void EventHandler::HandleEvent(MouseButtonReleasedEvent& event)
		{
			MouseButtonReleasedEvent MouseEvent = dynamic_cast<MouseButtonReleasedEvent&>(event);
			BLU_CORE_TRACE("Mouse Released");

		}
		void EventHandler::HandleEvent(KeyReleasedEvent& event)
		{
			KeyReleasedEvent MouseEvent = dynamic_cast<KeyReleasedEvent&>(event);
			BLU_CORE_TRACE("Key Released");
		}
		void EventHandler::HandleEvent(WindowResizeEvent& event)
		{
			
			Renderer::OnWindowResize(event.GetWidth(), event.GetHeight());
		}
		void EventHandler::HandleEvent(KeyTypedEvent& event)
		{
			BLU_CORE_TRACE("KeyTyped");;
		}
	}
	

}
