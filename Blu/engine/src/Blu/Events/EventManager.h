#pragma once
#include "Event.h"
#include <vector>
#include <iostream>

namespace Blu
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