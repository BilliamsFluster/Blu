#pragma once
#include "EventHandler.h"



namespace Blu
{
    namespace Events
    {
        class EventDispatcher
        {
        public:
            template <typename HandlerType, typename EventType>
            static void Dispatch(EventType& event)
            {
                HandlerType Handler;
                Handler.HandleTypedEvent<EventType>(event);
            }
        };
        
        
    }
	
}
#define DISPATCH_EVENT(handler, event) \
    do { \
        auto& typedEvent = event; \
        Blu::Events::EventDispatcher::Dispatch<handler, decltype(typedEvent)>(typedEvent); \
    } while (0)
