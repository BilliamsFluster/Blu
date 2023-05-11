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
                
                Events::KeyPressedEvent KeyEvent(key);
                DISPATCH_EVENT(Events::KeyPressedEventHandler, KeyEvent);
                


            }
            else if (action == GLFW_RELEASE)
            {
                Events::KeyReleasedEvent KeyEvent(key);
                DISPATCH_EVENT(Events::KeyReleasedEventHandler, KeyEvent);
            }
        }

        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
        {
            if (action == GLFW_PRESS)
            {
                Events::MouseButtonPressedEvent MousePressedEvent(button, action, mods);
                DISPATCH_EVENT(Events::MouseButtonPressedEventHandler, MousePressedEvent);
            }
            else if (action == GLFW_RELEASE)
            {
                Events::MouseButtonReleasedEvent MouseReleasedEvent(button, action, mods);
                DISPATCH_EVENT(Events::MouseButtonReleasedEventHandler, MouseReleasedEvent);
            }
            
        }

        static void MouseButtonScrolledCallback(GLFWwindow* window, double x_offset, double y_offset)
        {
            Events::MouseScrolledEvent ScrolledEvent(x_offset, y_offset);
            DISPATCH_EVENT(Events:: MouseScrolledEventHandler, ScrolledEvent);

        }
        static void WindowSizeCallback(GLFWwindow* window, int width, int height)
        {
            Events::WindowResizeEvent WindowEvent(width, height);
            DISPATCH_EVENT(Events::WindowResizeEventHandler, WindowEvent);
        }

        static void MouseMovedCallback(GLFWwindow* window, double xpos, double ypos)
        {
            Events::MouseMovedEvent MouseEvent(xpos, ypos);
            
            DISPATCH_EVENT(Events::MouseMovedEventHandler,MouseEvent);
        }
    }
}