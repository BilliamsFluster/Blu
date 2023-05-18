#include "Blupch.h"
#include "WindowsInput.h"
#include <GLFW/glfw3.h>
#include "Blu/Core/Application.h"

namespace Blu
{

	namespace WindowInput
	{
		Input* Input::s_Instance = new WindowsInput();;


		bool WindowsInput::IsKeyPressedImpl(int keycode)
		{
			GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
			auto state = glfwGetKey(window, keycode); 

			return state == GLFW_PRESS || state == GLFW_REPEAT; // is the key pressed or released
		}
		bool WindowsInput::IsMouseButtonPressedImpl(int button)
		{
			GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
			auto state = glfwGetMouseButton(window, button);

			return state == GLFW_PRESS;


		}
		std::pair<float, float> WindowsInput::GetMousePositionImpl()
		{
			GLFWwindow* window = (GLFWwindow*)Application::Get().GetWindow().GetNativeWindow();
			double xpos, ypos;

			glfwGetCursorPos(window, &xpos, &ypos);
			return { (float)xpos, (float)ypos };
		}

		
		float WindowsInput::GetMouseXImpl()
		{
			auto [x, y] = GetMousePositionImpl();

			return x;
		}
		float WindowsInput::GetMouseYImpl()
		{
			auto [x, y] = GetMousePositionImpl();

			return y;
			
		}
	}
}

