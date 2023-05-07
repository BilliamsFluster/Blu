#pragma once



namespace Blu
{
    namespace Events
    {
        class EventDispatcher
        {
        public:
            static void Dispatch(class Event& event);
           

        private:
            static void OnKeyEvent(class KeyEvent& event);
            static void OnMouseEvent(class MouseMovedEvent& event);
            static void OnWindowEvent(class WindowEvent& event);
            
        };
    }
	
}


