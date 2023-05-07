#pragma once
#include "Event.h"


namespace Blu
{
    namespace Events
    {
        class BLU_API KeyEvent : public Event
        {
        public:
            KeyEvent(int keycode)
                : m_KeyCode(keycode) {}

            inline int GetKeyCode() const { return m_KeyCode; }

            Type GetType() const override { return Type::KeyPressed; }

        private:
            int m_KeyCode;
        };
    }
    
}