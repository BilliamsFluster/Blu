#pragma once
#include "Event.h"
namespace Blu
{
    namespace Events
    {
        class BLU_API MouseMovedEvent : public Event
        {
        public:
            MouseMovedEvent(double x, double y)
                : m_MouseX(x), m_MouseY(y) {}

            inline double GetX() const { return m_MouseX; }
            inline double GetY() const { return m_MouseY; }

            virtual Type GetType() const override { return Type::MouseMoved; }
            const char* GetName() const override { return " MouseMoved"; }

        private:
            double m_MouseX, m_MouseY;
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
            MouseScrolledEvent(double x_offset, double y_offset)
                : m_XOffset(x_offset), m_YOffset(y_offset) {}

            inline double GetXOffset() const { return m_XOffset; }
            inline double GetYOffset() const { return m_YOffset; }

            virtual Type GetType() const override { return Type::MouseScrolled; }
            const char* GetName() const override { return " MouseScrolled"; }
        private:
            double m_XOffset, m_YOffset;
        };
    }
    
}