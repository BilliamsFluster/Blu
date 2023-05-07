#include "Blupch.h"
#include "EventDispatcher.h"
#include "Event.h"
#include "EventHandler.h"
#include "KeyEvent.h"
#include "MouseEvent.h"
#include "WindowEvent.h"



namespace Blu
{
    namespace Events
    {
         void EventDispatcher::Dispatch(Event& event)
        {

            switch (event.GetType())
            {
            case Event::Type::KeyPressed:
                OnKeyEvent(dynamic_cast<KeyEvent&>(event));
                break;
            case Event::Type::KeyReleased:
               

            case Event::Type::MouseMoved:
                 OnMouseEvent(dynamic_cast<MouseMovedEvent&>(event));
            case Event::Type::MouseButtonPressed:
            case Event::Type::MouseButtonReleased:
               
                break;
            case Event::Type::WindowResize:
            case Event::Type::WindowClose:
               // OnWindowEvent(dynamic_cast<WindowEvent&>(event));
                break;
            default:
                break;
            }
        }

         void EventDispatcher::OnKeyEvent(KeyEvent& event)
         {
             KeyEventHandler handler;
             handler.HandleEvent(event);
         }

         void EventDispatcher::OnMouseEvent(MouseMovedEvent& event)
         {
             MouseEventHandler handler;
             handler.HandleEvent(event);
         }

         void EventDispatcher::OnWindowEvent( WindowEvent& event)
         {
             //WindowEventHandler handler;
             //handler.HandleEvent(event);
         }
       
    }
    
}
