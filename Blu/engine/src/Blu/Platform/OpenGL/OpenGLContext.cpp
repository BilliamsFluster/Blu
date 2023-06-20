#include "Blupch.h"
#include "OpenGLContext.h"
#include "Blu/Core/Log.h"
#include <GLFW/glfw3.h>
#include <glad/glad.h>


namespace Blu
{
	OpenGLContext::OpenGLContext(GLFWwindow* windowHandle)
		:m_WindowHandle(windowHandle)
	{
	}
	void OpenGLContext::Init()
	{
		BLU_PROFILE_FUNCTION();

		glfwMakeContextCurrent(m_WindowHandle);
		int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

		std::cout << glGetString(GL_RENDERER) << std::endl;
		std::cout << glGetString(GL_VENDOR) << std::endl;
		std::cout << glGetString(GL_VERSION);

	}
	void OpenGLContext::SwapBuffers()
	{
		BLU_PROFILE_FUNCTION();
		glfwSwapBuffers(m_WindowHandle);

	}
}