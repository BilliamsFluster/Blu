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

		

	}
	void OpenGLContext::SwapBuffers()
	{
		BLU_PROFILE_FUNCTION();
		glfwSwapBuffers(m_WindowHandle);

	}
}