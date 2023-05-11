#pragma once
#include "Event.h"


namespace Blu
{
	namespace Events
	{
		class BLU_API EventHandler
		{
		public:
			virtual void HandleEvent(Event& event)
			{

			}

			template <typename EventType>
			void HandleTypedEvent(EventType& event)
			{
				HandleEvent(static_cast<EventType&>(event));
			}
		};

		class BLU_API WindowResizeEventHandler : public EventHandler
		{
		public:
			void HandleEvent(Event& event) override;
			
		};

		class BLU_API KeyPressedEventHandler : public EventHandler
		{
		public:
			void HandleEvent(Event& event) override;
		};

		class BLU_API KeyReleasedEventHandler : public EventHandler
		{
		public:
			void HandleEvent(Event& event) override;
		};

		class BLU_API MouseMovedEventHandler : public EventHandler
		{
		public:
			void HandleEvent(Event& event) override;
			
		};
		class BLU_API MouseScrolledEventHandler : public EventHandler
		{
		public:
			void HandleEvent(Event& event) override;

		};
		class BLU_API MouseButtonPressedEventHandler : public EventHandler
		{
		public:
			void HandleEvent(Event& event) override;

		};
		class BLU_API MouseButtonReleasedEventHandler : public EventHandler
		{
		public:
			void HandleEvent(Event& event) override;

		};
	}

	


}

