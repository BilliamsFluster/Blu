#pragma once
#include "Blu/Rendering/GraphicsContext.h"


struct GLFWwindow;

namespace Blu
{
	class OpenGLContext : public GraphicsContext
	{
	public:
		OpenGLContext(GLFWwindow* windowhandle);
		virtual void Init() override;
		virtual void SwapBuffers() override;
	private:
		GLFWwindow* m_WindowHandle;
	};
}