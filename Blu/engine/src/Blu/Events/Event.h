#pragma once
#include "Blu/Core/Core.h"


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

			virtual Type GetType() const = 0;
			bool Handled = false;


			virtual const char* GetName() const = 0;
		};
	}
	

#define EVENT_CLASS_TYPE(type) static Type GetStaticType(){return Type::type;}\
														virtual Type GetType() const override {return GetStaticType();}\
														//virtual const char* GetName() const override { return #type; } 


	

}

