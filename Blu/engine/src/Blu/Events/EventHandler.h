#pragma once
#include "Event.h"


namespace Blu
{
	namespace Events
	{
		class BLU_API EventHandler
		{
		public:
			virtual void HandleEvent(Event& event) = 0;
		};

		class BLU_API WindowEventHandler : public EventHandler
		{
		public:
			void HandleEvent(Event& event) override
			{
				// handle window event here...
			}
		};

		class BLU_API KeyEventHandler : public EventHandler
		{
		public:
			void HandleEvent(Event& event) override;
		};

		class BLU_API MouseEventHandler : public EventHandler
		{
		public:
			void HandleEvent(Event& event) override;
			
		};
	}

	


}

