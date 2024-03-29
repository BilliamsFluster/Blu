#include "Blupch.h"
#include "Blu/Core/Input.h"
#include <GLFW/glfw3.h>
#include "Blu/Core/Application.h"

namespace Blu
{
	int Blu::Input::m_KeyCode = 0;
	int Blu::Input::m_MouseCode = 0;
	bool Input::IsKeyPressed(int keycode)
	{
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		auto state = glfwGetKey(window, keycode);
		m_KeyCode = state;
		return state == GLFW_PRESS || state == GLFW_REPEAT; // is the key pressed or released
	}
	bool Input::IsMouseButtonPressed(int button)
	{
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		auto state = glfwGetMouseButton(window, button);
		m_MouseCode = state;
		return state == GLFW_PRESS;


	}
	std::pair<float, float> Input::GetMousePosition()
	{
		GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
		double xpos, ypos;

		glfwGetCursorPos(window, &xpos, &ypos);
		return { (float)xpos, (float)ypos };
	}


	float Input::GetMouseX()
	{
		auto [x, y] = GetMousePosition();

		return x;
	}
	float Input::GetMouseY()
	{
		auto [x, y] = GetMousePosition();

		return y;

	}
	
}