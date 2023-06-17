#include "Blupch.h"
#include "KeyEvent.h"

void Blu::Events::KeyPressedEvent::Accept(EventHandler& handler)
{
	handler.HandleEvent(*this);
}

void Blu::Events::KeyReleasedEvent::Accept(EventHandler& handler)
{
	handler.HandleEvent(*this);
}

void Blu::Events::KeyTypedEvent::Accept(EventHandler& handler)
{
	handler.HandleEvent(*this);
}
