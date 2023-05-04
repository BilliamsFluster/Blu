#pragma once
#include "Event.h"


namespace Blu
{
	class BLU_API EventHandler
	{
	public:
		virtual void HandleEvent(Event& event) = 0;
	};

	class BLU_API WindowEventHandler : public EventHandler
	{
	public:
		virtual void HandleEvent(Event& event) override
		{
			// handle window event here...
		}
	};

	class BLU_API InputEventHandler : public EventHandler
	{
	public:
		virtual void HandleEvent(Event& event) override
		{
			// handle input event here...
		}
	};
}

