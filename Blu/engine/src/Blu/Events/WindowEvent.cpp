#include "Blupch.h"
#include "WindowEvent.h"

void Blu::Events::WindowResizeEvent::Accept(EventHandler& handler)
{
	handler.HandleEvent(*this);
}
