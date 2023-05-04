#pragma once
#include "Event.h"


namespace Blu
{

    class BLU_API KeyPressedEvent : public Event
    {
    public:
        KeyPressedEvent(int keycode)
            : m_KeyCode(keycode) {}

        inline int GetKeyCode() const { return m_KeyCode; }

        virtual Type GetType() const override { return Type::KeyPressed; }

    private:
        int m_KeyCode;
    };

    class BLU_API KeyReleasedEvent : public Event
    {
    public:
        KeyReleasedEvent(int keycode)
            : m_KeyCode(keycode) {}

        inline int GetKeyCode() const { return m_KeyCode; }

        virtual Type GetType() const override { return Type::KeyReleased; }

    private:
        int m_KeyCode;
    };
}