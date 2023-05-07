#include "Blupch.h"
#include "KeyEvent.h"
#include "MouseEvent.h"
#include "WindowEvent.h"
#include "EventHandler.h"
#include "EventDispatcher.h"
#include <GLFW/glfw3.h>

namespace Blu
{
    namespace GLFWCallbacks
    {/*callback functions needs to be passed as function pointers to the GLFW library,
     it must have external linkage, which means it needs to be defined in a header file rather than a source file.*/
        static Events::EventDispatcher EventDispatcher;
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            if (action == GLFW_PRESS)
            {
                Events::KeyEvent keyEvent(key);
                EventDispatcher.Dispatch(keyEvent);


            }
            else if (action == GLFW_RELEASE)
            {



            }
        }

        void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
        {
            // Handle mouse button events here
        }

        void WindowSizeCallback(GLFWwindow* window, int width, int height)
        {
            // Handle window resize events here
        }

        void MouseMovedCallback(GLFWwindow* window, double xpos, double ypos)
        {
            Events::MouseMovedEvent MouseEvent(xpos, ypos);
            
            EventDispatcher.Dispatch(MouseEvent);
        }
    }
}