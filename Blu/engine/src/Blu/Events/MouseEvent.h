#pragma once
#include "Event.h"
namespace Blu
{
    namespace Events
    {
        class BLU_API MouseMovedEvent : public Event
        {
        public:
            MouseMovedEvent(float x, float y)
                : m_MouseX(x), m_MouseY(y) {}

            inline float GetX() const { return m_MouseX; }
            inline float GetY() const { return m_MouseY; }

            virtual Type GetType() const override { return Type::MouseMoved; }

        private:
            float m_MouseX, m_MouseY;
        };

        class BLU_API MouseButtonPressedEvent : public Event
        {
        public:
            MouseButtonPressedEvent(int button)
                : m_Button(button) {}

            inline int GetButton() const { return m_Button; }

            Type GetType() const override { return Type::MouseButtonPressed; }

        private:
            int m_Button;
        };

        class BLU_API MouseButtonReleasedEvent : public Event
        {
        public:
            MouseButtonReleasedEvent(int button)
                : m_Button(button) {}

            inline int GetButton() const { return m_Button; }

            virtual Type GetType() const override { return Type::MouseButtonReleased; }

        private:
            int m_Button;
        };

        class BLU_API MouseScrolledEvent : public Event
        {
        public:
            MouseScrolledEvent(float x_offset, float y_offset)
                : m_XOffset(x_offset), m_YOffset(y_offset) {}

            inline float GetXOffset() const { return m_XOffset; }
            inline float GetYOffset() const { return m_YOffset; }

            virtual Type GetType() const override { return Type::MouseScrolled; }

        private:
            float m_XOffset, m_YOffset;
        };
    }
    
}