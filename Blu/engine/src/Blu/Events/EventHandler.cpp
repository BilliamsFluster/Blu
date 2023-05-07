#pragma once
#include "Blupch.h"
#include "KeyEvent.h"
#include "MouseEvent.h"
#include "EventHandler.h"
#include "Blu/Core/KeyCodes.h"

namespace Blu
{
	namespace Events
	{
		void KeyEventHandler::HandleEvent(Event& event)
		{

			KeyEvent& keyEvent = dynamic_cast<KeyEvent&>(event);
			
			Key::KeyString(keyEvent.GetKeyCode());
		}

		void MouseEventHandler::HandleEvent(Event& event)
		{
			MouseMovedEvent& MouseEvent = dynamic_cast<MouseMovedEvent&>(event);

			std::cout << "Mouse Pos X: "<<  MouseEvent.GetX() <<"Mouse Pos Y: "<< MouseEvent.GetY() <<std::endl;


		}
	}
	

}
