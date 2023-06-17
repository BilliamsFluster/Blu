#include "Blupch.h"
#include "RenderCommand.h"
#include "Blu/Platform/OpenGL/OpenGLRendererAPI.h"

namespace Blu
{
	RendererAPI* RenderCommand::s_RendererAPI = new OpenGLRendererAPI;
	
	
}