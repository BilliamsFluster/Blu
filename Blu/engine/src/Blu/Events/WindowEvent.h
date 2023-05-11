#pragma once
#include "Event.h"
namespace Blu
{

    namespace Events
    {
        class BLU_API WindowResizeEvent : public Event
        {
        public:
            WindowResizeEvent(int width, int height)
                : m_Width(width), m_Height(height) {}

            inline int GetWidth() const { return m_Width; }
            inline int GetHeight() const { return m_Height; }

            EVENT_CLASS_TYPE(WindowResize)
        private:
            int m_Width, m_Height;
        };
    }
    
}