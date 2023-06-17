#pragma once
#include "Blu/Core/Core.h"
#include "EventHandler.h"



namespace Blu
{
	namespace Events
	{
		class  BLU_API Event
		{
		public:
			/* Define Event Types*/
			enum class Type
			{
				None = 0,
				KeyPressed, KeyReleased, KeyTyped,
				MouseMoved, MouseButtonPressed, MouseButtonReleased, MouseScrolled,
				WindowResize, WindowClose
			};

			virtual Type GetType() const  = 0;
			virtual void Accept(EventHandler& handler) = 0;
			bool Handled = false;


			virtual const char* GetName() const = 0;
		};
	}
	



	

}

