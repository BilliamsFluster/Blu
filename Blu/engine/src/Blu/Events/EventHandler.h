#pragma once
#include "Blu/Core/Core.h"


namespace Blu
{
	namespace Events
	{
		class BLU_API EventHandler
		{
		public:

			template<typename EventType>
			void HandleTypedEvent(EventType& event)
			{
				HandleEvent(event);
			}
			void HandleEvent(class KeyPressedEvent& event);


			void HandleEvent(class MouseMovedEvent & event);
			
			void HandleEvent(class MouseScrolledEvent& event);
			

			void HandleEvent(class MouseButtonPressedEvent& event);
			
			void HandleEvent(class MouseButtonReleasedEvent& event);
			
			void HandleEvent(class KeyReleasedEvent& event);

			void HandleEvent(class WindowResizeEvent& event);
			
			void HandleEvent(class KeyTypedEvent& event);
			
		};

	}

	


}

