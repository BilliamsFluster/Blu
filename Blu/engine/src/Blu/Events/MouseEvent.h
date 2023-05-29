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
            const char* GetName() const override { return " MouseMoved"; }

        private:
            float m_MouseX, m_MouseY;
        };


        class BLU_API MouseButtonPressedEvent : public Event
        {
        public:
            MouseButtonPressedEvent(int button, int action, int mods)
                : m_Button(button), m_Action(action), m_Mods(mods) {}

            inline int GetButton() const { return m_Button; }
            inline int GetAction() const { return m_Action; }
            inline int GetMods() const { return m_Mods; }
            Type GetType() const override { return Type::MouseButtonPressed; }
            const char* GetName() const override { return " MousePressed"; }
        private:
            int m_Button, m_Action, m_Mods;
        };


        class BLU_API MouseButtonReleasedEvent : public Event
        {
        public:
            MouseButtonReleasedEvent(int button, int action, int mods)
                : m_Button(button), m_Action(action), m_Mods(mods) {}

            inline int GetButton() const { return m_Button; }
            inline int GetAction() const { return m_Action; }
            inline int GetMods() const { return m_Mods; }

            virtual Type GetType() const override { return Type::MouseButtonReleased; }
            const char* GetName() const override { return " MouseReleased"; }
        private:
            int m_Button, m_Action, m_Mods;
        };


        class BLU_API MouseScrolledEvent : public Event
        {
        public:
            MouseScrolledEvent(float x_offset, float y_offset)
                : m_XOffset(x_offset), m_YOffset(y_offset) {}

            inline float GetXOffset() const { return m_XOffset; }
            inline float GetYOffset() const { return m_YOffset; }

            virtual Type GetType() const override { return Type::MouseScrolled; }
            const char* GetName() const override { return " MouseScrolled"; }
        private:
            float m_XOffset, m_YOffset;
        };
    }
    
}