#pragma once
#include "Event.h"


namespace Blu
{
    namespace Events
    {
        class BLU_API KeyPressedEvent : public Event
        {
        public:
            KeyPressedEvent(int keycode)
                : m_KeyCode(keycode) {}

            inline int GetKeyCode() const { return m_KeyCode; }

            Type GetType() const override { return Type::KeyPressed; }
            const char* GetName() const override { return " KeyPressed"; }

        private:
            int m_KeyCode;
        };

        class BLU_API KeyReleasedEvent : public Event
        {
        public:
            KeyReleasedEvent(int keycode)
                : m_KeyCode(keycode) {}

            inline int GetKeyCode() const { return m_KeyCode; }

            Type GetType() const override { return Type::KeyReleased; }
            const char* GetName() const override { return " KeyReleased"; }

        private:
            int m_KeyCode;
        };

        class BLU_API KeyTypedEvent : public Event
        {
        public:
            KeyTypedEvent(int keycode)
                : m_KeyCode(keycode) {}

            inline int GetKeyCode() const { return m_KeyCode; }

            Type GetType() const override { return Type::KeyTyped; }
            const char* GetName() const override { return " KeyReleased"; }

        private:
            int m_KeyCode;
        };
    }
    
}