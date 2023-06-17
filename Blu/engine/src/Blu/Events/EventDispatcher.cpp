#include "Blupch.h"
#include "EventDispatcher.h"
#include "Event.h"
#include "EventHandler.h"
#include "KeyEvent.h"
#include "MouseEvent.h"
#include "WindowEvent.h"



namespace Blu
{
    namespace Events
    {
        EventDispatcher* m_Instance = new EventDispatcher();
        
    }
    
}
