#pragma once
#include "KeyEvent.h"
#include "MouseEvent.h"
#include "WindowEvent.h"
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

           

        private:
           /* static void OnKeyPressedEvent( KeyPressedEvent& event);
            static void OnKeyReleasedEvent( KeyReleasedEvent& event);
            static void OnMouseMovedEvent( MouseMovedEvent& event);
            static void OnMousePressedEvent( MouseButtonPressedEvent& event);
            static void OnMouseReleasedEvent( MouseButtonReleasedEvent& event);
            static void OnMouseScrolledEvent( MouseScrolledEvent& event);*/


           // static void OnWindowEvent(WindowEvent& event);
            
        };
        
        
    }
	
}
#define DISPATCH_EVENT(handler, event) \
    do { \
        auto& typedEvent = event; \
        Blu::Events::EventDispatcher::Dispatch<handler, decltype(typedEvent)>(typedEvent); \
    } while (0)
