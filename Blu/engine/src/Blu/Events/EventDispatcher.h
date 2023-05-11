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
                static EventDispatcher instance;
                return instance;
            }

            
            void Dispatch(EventHandler& handler, Event& event, Layers::LayerStack& layerStack)
            {
                /*for (auto layer : layerStack)
                {
                    
                    if (event.Handled)
                        break;

                    layer->OnEvent(handler, event);

                    if (event.Handled)
                        break;
                }*/

                for (auto it = layerStack.rbegin(); it != layerStack.rend(); ++it)
                {
                    (*it)->OnEvent(handler, event);
                    if (event.Handled)
                        break;
                }
            }

            

        private:
           
        };
    }
}
#define DISPATCH_EVENT(handler, event) \
    do { \
        auto& typedEvent = event; \
        Blu::Application::Get().GetEventDispatcher().Dispatch(handler, event, Blu::Application::Get().GetLayerStack()); \
    } while (0)
