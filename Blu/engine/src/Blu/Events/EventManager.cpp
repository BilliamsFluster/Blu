#include "EventManager.h"

namespace Blu
{

    void EventManager::AddEventHandler(EventHandler* handler)
    {
        m_EventHandlers.push_back(handler);
    }

    void EventManager::RemoveEventHandler(EventHandler* handler)
    {
        auto it = std::find(m_EventHandlers.begin(), m_EventHandlers.end(), handler);
        if (it != m_EventHandlers.end())
            m_EventHandlers.erase(it);
    }

    void EventManager::HandleEvent(Event& event)
    {
        for (auto handler : m_EventHandlers)
        {
            handler->HandleEvent(event);
        }
    }
}