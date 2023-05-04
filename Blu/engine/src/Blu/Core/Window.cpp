#include "Window.h"


namespace Blu
{
	Window::Window(int width, int height, const std::string& title)
	{
	}
	Window::~Window()
	{
	}
	bool Blu::Window::pollEvent(Event& event)
	{
		return false;
	}

	LRESULT Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
        switch (uMsg)
        {
        case WM_LBUTTONDOWN:
        {
            POINTS pt = MAKEPOINTS(lParam);
            Vec2 position(pt.x, pt.y);
            window.onMouseButtonPressed(MouseButton::Left, position);
            break;
        }

        case WM_RBUTTONDOWN:
        {
            POINTS pt = MAKEPOINTS(lParam);
            Vec2 position(pt.x, pt.y);
            window.onMouseButtonPressed(MouseButton::Right, position);
            break;
        }

        case WM_MBUTTONDOWN:
        {
            POINTS pt = MAKEPOINTS(lParam);
            Vec2 position(pt.x, pt.y);
            window.onMouseButtonPressed(MouseButton::Middle, position);
            break;
        }

        // other message cases here...
        // ...
        }

        // default message handling
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

}