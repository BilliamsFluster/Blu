#include "Blupch.h"
#include "KeyEvent.h"
#include "MouseEvent.h"
#include "WindowEvent.h"
#include "EventHandler.h"
#include "EventDispatcher.h"
#include <GLFW/glfw3.h>
#include "Blu/Core/Application.h"

namespace Blu
{
    namespace GLFWCallbacks
    {/*callback functions needs to be passed as function pointers to the GLFW library,
     it must have external linkage, which means it needs to be defined in a header file rather than a source file.*/
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            if (action == GLFW_PRESS)
            {
                
                Events::KeyPressedEvent KeyEvent(key);
                Events::KeyPressedEventHandler Handler;

                DISPATCH_EVENT(Handler, KeyEvent);
                


            }
            else if (action == GLFW_RELEASE)
            {
                Events::KeyReleasedEvent KeyEvent(key);
                Events::KeyReleasedEventHandler Handler;
                DISPATCH_EVENT(Handler, KeyEvent);
            }
        }

        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
        {
            if (action == GLFW_PRESS)
            {
                Events::MouseButtonPressedEvent MousePressedEvent(button, action, mods);
                Events::MouseButtonPressedEventHandler Handler;
                DISPATCH_EVENT(Handler, MousePressedEvent);
            }
            else if (action == GLFW_RELEASE)
            {
                Events::MouseButtonReleasedEvent MouseReleasedEvent(button, action, mods);
                Events::MouseButtonReleasedEventHandler Handler;
                DISPATCH_EVENT(Handler, MouseReleasedEvent);
            }
            
        }

        static void CharCallback(GLFWwindow* window, unsigned int keycode)
        {
            Events::KeyTypedEvent KeyEvent(keycode);
            Events::KeyTypedEventHandler Handler;

            DISPATCH_EVENT(Handler, KeyEvent);
        }

        static void MouseButtonScrolledCallback(GLFWwindow* window, double x_offset, double y_offset)
        {
            Events::MouseScrolledEvent ScrolledEvent(x_offset, y_offset);
            Events::MouseScrolledEventHandler Handler;
            DISPATCH_EVENT(Handler, ScrolledEvent);

        }
        static void WindowSizeCallback(GLFWwindow* window, int width, int height)
        {
            Events::WindowResizeEvent WindowEvent(width, height);
            Events::WindowResizeEventHandler Handler;
            DISPATCH_EVENT(Handler, WindowEvent);
        }

        static void MouseMovedCallback(GLFWwindow* window, double xpos, double ypos)
        {
            Events::MouseMovedEvent MouseEvent(xpos, ypos);
            Events::MouseMovedEventHandler Handler;
            DISPATCH_EVENT(Handler, MouseEvent);
        }
    }
}