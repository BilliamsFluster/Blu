#pragma once
#include "Event.h"
#include "EventHandler.h"
#include <vector>
#include <iostream>

namespace Blu
{
    namespace Events
    {
        class BLU_API EventManager
        {
        public:
            EventManager() = default;

            void AddEventHandler(EventHandler* handler);
            void RemoveEventHandler(EventHandler* handler);

            void HandleEvent(Event& event);

        private:
            std::vector<EventHandler*> m_EventHandlers;
        };
    }

   
}