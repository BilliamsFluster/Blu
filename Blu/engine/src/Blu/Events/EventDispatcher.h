#pragma once
#include "EventHandler.h"
#include "Blu/Core/LayerStack.h"



namespace Blu
{
    namespace Events
    {
        class EventDispatcher
        {
        public:
            static EventDispatcher& GetInstance()
            {
                
                return *m_Instance;
            }
            
            
            void Dispatch(Event& event, Layers::LayerStack& layerStack)
            {
                for (auto it = layerStack.rbegin(); it != layerStack.rend(); ++it)
                {
                    (*it)->OnEvent(event);
                    if (event.Handled)
                        break;
                }
            }

            

        private:
            static EventDispatcher* m_Instance;
        };
    }
}
#define DISPATCH_EVENT(event) \
    do { \
        auto& typedEvent = event; \
        Blu::Application::Get().GetEventDispatcher().Dispatch(event, Blu::Application::Get().GetLayerStack()); \
    } while (0)

#define BIND_EVENT_FN(x) std::bind(&x, this, std::placeholders::_1) // for use for the newly refactored event system

