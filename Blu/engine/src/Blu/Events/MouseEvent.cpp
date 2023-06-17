#include "Blupch.h"
#include "MouseEvent.h"

void Blu::Events::MouseMovedEvent::Accept(EventHandler& handler)
{
	handler.HandleEvent(*this);
}

void Blu::Events::MouseButtonPressedEvent::Accept(EventHandler& handler)
{
	handler.HandleEvent(*this);

}

void Blu::Events::MouseButtonReleasedEvent::Accept(EventHandler& handler)
{
	handler.HandleEvent(*this);

}

void Blu::Events::MouseScrolledEvent::Accept(EventHandler& handler)
{
	handler.HandleEvent(*this);

}
