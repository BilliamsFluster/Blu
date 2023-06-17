#pragma once
#include "Event.h"
namespace Blu
{

    namespace Events
    {
        class BLU_API WindowResizeEvent : public Event
        {
        public:
            WindowResizeEvent(float width, float height)
                : m_Width(width), m_Height(height) {}

            inline float GetWidth() const { return m_Width; }
            inline float GetHeight() const { return m_Height; }

            virtual Type GetType() const override { return Type::WindowResize; }
            virtual void Accept(EventHandler& handler) override;


           // EVENT_CLASS_TYPE(WindowResize);

            const char* GetName() const override { return " WindowResize"; }
        private:
            float m_Width, m_Height;
        };
    }
    
}